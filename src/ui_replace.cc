//------------------------------------------------------------------------
//  FIND AND REPLACE
//------------------------------------------------------------------------
//
//  Eureka DOOM Editor
//
//  Copyright (C) 2015 Andrew Apted
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 2
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//------------------------------------------------------------------------

#include "main.h"
#include "ui_window.h"

#include "e_path.h"  // GoToObject
#include "m_game.h"
#include "w_rawdef.h"


class number_group_c
{
	// This represents a small group of numbers and number ranges,
	// which the user can type into the Match input box.
#define NUMBER_GROUP_MAX	40

private:
	int size;

	int ranges[NUMBER_GROUP_MAX][2];

	bool everything;

public:
	number_group_c() : size(0), everything(false)
	{ }

	~number_group_c()
	{ }

	void clear()
	{
		size = 0;
		everything = false;
	}

	bool is_single() const
	{
		return (size == 1) && (ranges[0][0] == ranges[0][1]);
	}

	bool is_everything() const
	{
		return everything;
	}

	int grab_first() const
	{
		if (size == 0)
			return 0;

		return ranges[0][0];
	}

	void insert(int low, int high)
	{
		// overflow is silently ignored
		if (size >= NUMBER_GROUP_MAX)
			return;

		// TODO : try to merge with existing range

		ranges[size][0] = low;
		ranges[size][1] = high;

		size++;
	}

	bool get(int num) const
	{
		for (int i = 0 ; i < size ; i++)
		{
			if (ranges[i][0] <= num && num <= ranges[i][1])
				return true;
		}

		return false;
	}

	//
	// Parse a string like "1,3-5,9" and add the numbers (or ranges)
	// to this group.  Returns false for malformed strings.
	// An empty string is considered invalid.
	//
	bool ParseString(const char *str)
	{
		char *endptr;

		for (;;)
		{
			// support an asterix to mean everything
			// (useful when using filters)
			if (*str == '*')
			{
				insert(INT_MIN, INT_MAX);
				everything = true;
				return true;
			}

			int low  = (int)strtol(str, &endptr, 0 /* allow hex */);
			int high = low;

			if (endptr == str)
				return false;

			str = endptr;

			while (isspace(*str))
				str++;

			// check for range
			if (*str == '-' || (str[0] == '.' && str[1] == '.'))
			{
				str += (*str == '-') ? 1 : 2;

				while (isspace(*str))
					str++;

				high = (int)strtol(str, &endptr, 0 /* allow hex */);

				if (endptr == str)
					return false;

				str = endptr;

				// valid range?
				if (high < low)
					return false;

				while (isspace(*str))
					str++;
			}

			insert(low, high);

			if (*str == 0)
				return true;  // OK //

			// valid separator?
			if (*str == ',' || *str == '/' || *str == '|')
				str++;
			else
				return false;
		}
	}
};


//------------------------------------------------------------------------

class UI_TripleCheckButton : public Fl_Group
{
	/* this button has three states to represent how a search should
	   check against a boolean value:

	     1. want value to be FALSE   (show with red 'X')
	     2. want value to be TRUE    (show with green tick)
	     3. don't care about value   (show with black '?')
	*/

private:
	int _value;	  // -1, 0, +1

	Fl_Button * false_but;
	Fl_Button *  true_but;
	Fl_Button * other_but;

	void Update()
	{
		false_but->hide();
		 true_but->hide();
		other_but->hide();

		if (_value < 0)
			false_but->show();
		else if (_value > 0)
			true_but->show();
		else
			other_but->show();

		redraw();
	}

	void BumpValue()
	{
		_value = (_value  < 0) ? 0 : (_value == 0) ? 1 : -1;
		Update();
	}

	static void button_callback(Fl_Widget *w, void *data)
	{
		UI_TripleCheckButton *G = (UI_TripleCheckButton *)data;

		G->BumpValue();
		G->do_callback();
	}

public:
	UI_TripleCheckButton(int X, int Y, int W, int H, const char *label = NULL) :
		Fl_Group(X, Y, W, H),
		_value(0)
	{
		if (label)
		{
			Fl_Box *box = new Fl_Box(FL_NO_BOX, X, Y, W, H, label);
			box->align(FL_ALIGN_LEFT);
		}

		false_but = new Fl_Button(X, Y, W, H, "N");
		false_but->labelcolor(FL_RED);
		false_but->labelsize(H*2/3);
		false_but->callback(button_callback, this);

		true_but = new Fl_Button(X, Y, W, H, "Y");
		true_but->labelfont(FL_HELVETICA_BOLD);
		true_but->labelcolor(fl_rgb_color(0, 176, 0));
		true_but->labelsize(H*2/3);
		true_but->callback(button_callback, this);

		other_but = new Fl_Button(X, Y, W, H, "-");
		other_but->labelsize(H*3/4);
		other_but->callback(button_callback, this);

		end();

		resizable(NULL);

		Update();
	}

	virtual ~UI_TripleCheckButton()
	{ }

public:
	int value() const { return _value; }

	void value(int new_value)
	{
		_value = new_value;
		Update();
	}
};


//------------------------------------------------------------------------

UI_FindAndReplace::UI_FindAndReplace(int X, int Y, int W, int H) :
	Fl_Group(X, Y, W, H, NULL),
	find_numbers(new number_group_c),
	 tag_numbers(new number_group_c),
	cur_obj(OBJ_THINGS, -1)
{
	box(FL_FLAT_BOX);

	color(WINDOW_BG, WINDOW_BG);

	
	/* ---- FIND AREA ---- */

	Fl_Group *grp1 = new Fl_Group(X, Y, W, 210);
	grp1->box(FL_UP_BOX);
	{
		Fl_Box *title = new Fl_Box(X + 50, Y + 10, W - 60, 30, "Find and Replace");
		title->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
		title->labelsize(18+KF*4);


		what = new Fl_Choice(X+60, Y+46, W - 120, 33);
		what->textsize(17);
		what->add("Things|Line Textures|Sector Flats|Lines by Type|Sectors by Type");
		what->value(0);
		what->callback(what_kind_callback, this);

		UpdateWhatColor();


		find_match = new Fl_Input(X+70, Y+95, 125, 25, "Match: ");
		find_match->when(FL_WHEN_CHANGED);
		find_match->callback(find_match_callback, this);

		find_choose = new Fl_Button(X+210, Y+95, 70, 25, "Choose");
		find_choose->callback(find_choose_callback, this);

		find_desc = new Fl_Output(X+70, Y+125, 210, 25, "Desc: ");

		find_but = new Fl_Button(X+50, Y+165, 80, 30, "Find");
		find_but->labelfont(FL_HELVETICA_BOLD);
		find_but->callback(find_but_callback, this);

		select_all_but = new Fl_Button(X+165, Y+165, 93, 30, "Select All");
		select_all_but->callback(select_all_callback, this);
	}
	grp1->end();


	/* ---- REPLACE AREA ---- */

	Fl_Group *grp2 = new Fl_Group(X, Y + 214, W, 132);
	grp2->box(FL_UP_BOX);
	{
		rep_value = new Fl_Input(X+80, Y+230, 115, 25, "New val: ");
		rep_value->when(FL_WHEN_CHANGED);
		rep_value->callback(rep_value_callback, this);

		rep_choose = new Fl_Button(X+210, Y+230, 70, 25, "Choose");
		rep_choose->callback(rep_choose_callback, this);

		rep_desc = new Fl_Output(X+80, Y+260, 200, 25, "Desc: ");

		apply_but = new Fl_Button(X+45, Y+300, 90, 30, "Replace");
		apply_but->labelfont(FL_HELVETICA_BOLD);
		apply_but->callback(apply_but_callback, this);

		replace_all_but = new Fl_Button(X+160, Y+300, 105, 30, "Replace All");
		replace_all_but->callback(replace_all_callback, this);
	}
	grp2->end();


	/* ---- FILTER AREA ---- */

	Fl_Group *grp3 = new Fl_Group(X, Y + 350, W, H - 350);
	grp3->box(FL_UP_BOX);
	{
		filter_toggle = new Fl_Toggle_Button(X+15, Y+356, 30, 30, "v");
		filter_toggle->labelsize(16);
		filter_toggle->color(FL_DARK3, FL_DARK3);
		filter_toggle->callback(filter_toggle_callback, this);
		filter_toggle->clear_visible_focus();

		Fl_Box *f_text = new Fl_Box(X+60, Y+356, 200, 30, "Search Filters");
		f_text->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
		f_text->labelsize(16);

		filter_group = new Fl_Group(X, Y+391, W, H-391);
		{
			// common stuff
			tag_input = new Fl_Input(X+105, Y+390, 130, 24, "Tag match:");
			tag_input->when(FL_WHEN_CHANGED);
			tag_input->callback(tag_input_callback, this);

			// thing stuff
			o_easy   = new UI_TripleCheckButton(X+105, Y+414, 28, 26, "easy: ");
			o_medium = new UI_TripleCheckButton(X+105, Y+444, 28, 26, "medium: ");
			o_hard   = new UI_TripleCheckButton(X+105, Y+474, 28, 26, "hard: ");

			o_sp     = new UI_TripleCheckButton(X+220, Y+414, 28, 26, "sp: ");
			o_coop   = new UI_TripleCheckButton(X+220, Y+444, 28, 26, "coop: ");
			o_dm     = new UI_TripleCheckButton(X+220, Y+474, 28, 26, "dm: ");

			// sector stuff
			o_floors   = new Fl_Check_Button(X+45, Y+418, 80, 22, " floors");
			o_ceilings = new Fl_Check_Button(X+45, Y+440, 80, 22, " ceilings");

			// linedef stuff
			o_lowers  = new Fl_Check_Button(X+45, Y+418, 80, 22, " lowers");
			o_uppers  = new Fl_Check_Button(X+45, Y+440, 80, 22, " uppers");
			o_rail    = new Fl_Check_Button(X+45, Y+462, 80, 22, " rail");

			o_one_sided = new Fl_Check_Button(X+155, Y+418, 80, 22, " one-sided");
			o_two_sided = new Fl_Check_Button(X+155, Y+440, 80, 22, " two-sided");
		}
		filter_group->end();
		filter_group->hide();

		UpdateWhatFilters();
	}
	grp3->end();


	grp3->resizable(NULL);
	resizable(grp3);

	end();
}


UI_FindAndReplace::~UI_FindAndReplace()
{ }


void UI_FindAndReplace::UpdateWhatColor()
{
	switch (what->value())
	{
		case 0: /* Things      */ what->color(FL_MAGENTA); break;
		case 1: /* Line Tex    */ what->color(fl_rgb_color(0,128,255)); break;
		case 2: /* Sector Flat */ what->color(FL_YELLOW); break;
		case 3: /* Line Type   */ what->color(FL_GREEN); break;
		case 4: /* Sector Type */ what->color(fl_rgb_color(255,144,0)); break;
	}
}



void UI_FindAndReplace::UpdateWhatFilters()
{
	int x = what->value();

#define SHOW_WIDGET_IF(w, test)  \
	if (test) (w)->show(); else (w)->hide();

	// common stuff
	if (x != 0)
		tag_input->activate();
	else
		tag_input->deactivate();

	// thing stuff
	SHOW_WIDGET_IF(o_easy,   x == 0);
	SHOW_WIDGET_IF(o_medium, x == 0);
	SHOW_WIDGET_IF(o_hard,   x == 0);

	SHOW_WIDGET_IF(o_sp,     x == 0);
	SHOW_WIDGET_IF(o_coop,   x == 0);
	SHOW_WIDGET_IF(o_dm,     x == 0);

	// sector stuff
	SHOW_WIDGET_IF(o_floors,   x == 2);
	SHOW_WIDGET_IF(o_ceilings, x == 2);

	// linedef stuff
	SHOW_WIDGET_IF(o_lowers, x == 1);
	SHOW_WIDGET_IF(o_uppers, x == 1);
	SHOW_WIDGET_IF(o_rail,   x == 1);

	SHOW_WIDGET_IF(o_one_sided, x == 1 || x == 3);
	SHOW_WIDGET_IF(o_two_sided, x == 1 || x == 3);

#undef SHOW_WIDGET_IF

	// vanilla DOOM : always hide SP and COOP flags
	if (x == 0 && ! game_info.coop_dm_flags)
	{
		  o_sp->hide();
		o_coop->hide();
	}
}


void UI_FindAndReplace::rawShowFilter(int value)
{
	if (value)
	{
		filter_toggle->label("^");
		filter_group->show();
	}
	else
	{
		filter_toggle->label("v");
		filter_group->hide();
	}
}


void UI_FindAndReplace::filter_toggle_callback(Fl_Widget *w, void *data)
{
	UI_FindAndReplace *box = (UI_FindAndReplace *)data;
		
	Fl_Toggle_Button *toggle = (Fl_Toggle_Button *)w;

	box->rawShowFilter(toggle->value());
}


void UI_FindAndReplace::what_kind_callback(Fl_Widget *w, void *data)
{
	UI_FindAndReplace *box = (UI_FindAndReplace *)data;

	box->Clear();

	bool want_descs = true;

	switch (box->what->value())
	{
		case 0: box->cur_obj.type = OBJ_THINGS; break;
		case 1: box->cur_obj.type = OBJ_LINEDEFS; want_descs = false; break;
		case 2: box->cur_obj.type = OBJ_SECTORS;  want_descs = false; break;
		case 3: box->cur_obj.type = OBJ_LINEDEFS; break;
		case 4: box->cur_obj.type = OBJ_SECTORS; break;

		default: break;
	}

	box->UpdateWhatColor();
	box->UpdateWhatFilters();

	if (want_descs)
	{
		box->find_desc->activate();
		box-> rep_desc->activate();
	}
	else
	{
		box->find_desc->deactivate();
		box-> rep_desc->deactivate();
	}
}


void UI_FindAndReplace::Open()
{
	show();

	WhatFromEditMode();

	// this will do a Clear() for us
	what->do_callback();

	Fl::focus(find_match);
}


void UI_FindAndReplace::Clear()
{
	cur_obj.clear();

	find_match->value("");
	find_desc->value("");
	find_but->label("Find");

	rep_value->value("");
	rep_desc->value("");

	find_but->deactivate();
	select_all_but->deactivate();

	apply_but->deactivate();
	replace_all_but->deactivate();

	rawShowFilter(0);
}


bool UI_FindAndReplace::WhatFromEditMode()
{
	switch (edit.mode)
	{
		case OBJ_THINGS:   what->value(0); return true;
		case OBJ_LINEDEFS: what->value(1); return true;
		case OBJ_SECTORS:  what->value(2); return true;

		default: return false;
	}
}


void UI_FindAndReplace::find_but_callback(Fl_Widget *w, void *data)
{
	UI_FindAndReplace *box = (UI_FindAndReplace *)data;

	box->FindNext();
}


void UI_FindAndReplace::select_all_callback(Fl_Widget *w, void *data)
{
	UI_FindAndReplace *box = (UI_FindAndReplace *)data;

	box->DoAll(false /* replace */);
}


void UI_FindAndReplace::apply_but_callback(Fl_Widget *w, void *data)
{
	UI_FindAndReplace *box = (UI_FindAndReplace *)data;

	box->DoReplace();
}


void UI_FindAndReplace::replace_all_callback(Fl_Widget *w, void *data)
{
	UI_FindAndReplace *box = (UI_FindAndReplace *)data;

	box->DoAll(true /* replace */);
}


void UI_FindAndReplace::find_match_callback(Fl_Widget *w, void *data)
{
	UI_FindAndReplace *box = (UI_FindAndReplace *)data;

	bool is_valid = box->CheckInput(box->find_match, box->find_desc, box->find_numbers);

	if (is_valid)
	{
		box->find_but->activate();
		box->select_all_but->activate();

		box->find_match->textcolor(FL_FOREGROUND_COLOR);
		box->find_match->redraw();
	}
	else
	{
		box->find_but->deactivate();
		box->select_all_but->deactivate();

		box->find_match->textcolor(FL_RED);
		box->find_match->redraw();
	}

	// update Replace section too
	box->rep_value->do_callback();
}

void UI_FindAndReplace::rep_value_callback(Fl_Widget *w, void *data)
{
	UI_FindAndReplace *box = (UI_FindAndReplace *)data;

	bool is_valid = box->CheckInput(box->rep_value, box->rep_desc);

	if (is_valid)
	{
		box->rep_value->textcolor(FL_FOREGROUND_COLOR);
		box->rep_value->redraw();
	}
	else
	{
		box->rep_value->textcolor(FL_RED);
		box->rep_value->redraw();
	}

	bool is_usable = (is_valid && box->find_but->active());

	if (is_usable)
		box->replace_all_but->activate();
	else
		box->replace_all_but->deactivate();

	// require an found object too before 'Replace' button can be used

	if (box->cur_obj.is_nil())
		is_usable = false;

	if (is_usable)
		box->apply_but->activate();
	else
		box->apply_but->deactivate();
}


bool UI_FindAndReplace::CheckInput(Fl_Input *w, Fl_Output *desc, number_group_c *num_grp)
{
	if (strlen(w->value()) == 0)
	{
		desc->value("");
		return false;
	}

	if (what->value() == 1 || what->value() == 2)
		return true;


	// for numeric types, parse the number(s) and/or ranges

	int type_num;

	if (! num_grp)
	{
		// just check the number is valid
		char *endptr;

		type_num = strtol(w->value(), &endptr, 0 /* allow hex */);

		if (*endptr != 0)
		{
			desc->value("(parse error)");
			return false;
		}
	}
	else
	{
		num_grp->clear();

		if (! num_grp->ParseString(w->value()))
		{
			desc->value("(parse error)");
			return false;
		}

		if (num_grp->is_everything())
		{
			desc->value("(everything)");
			return true;
		}
		else if (! num_grp->is_single())
		{
			desc->value("(multi-match)");
			return true;
		}

		type_num = num_grp->grab_first();
	}


	switch (what->value())
	{
		case 0: // Things
		{
			const thingtype_t *info = M_GetThingType(type_num);
			desc->value(info->desc);
			break;
		}

		case 3: // Lines by Type
		{
			const linetype_t *info = M_GetLineType(type_num);
			desc->value(info->desc);
			break;
		}

		case 4: // Lines by Type
		{
			const sectortype_t * info = M_GetSectorType(type_num);
			desc->value(info->desc);
			break;
		}

		default: break;
	}

	return true;
}


void UI_FindAndReplace::tag_input_callback(Fl_Widget *w, void *data)
{
	UI_FindAndReplace *box = (UI_FindAndReplace *)data;

	bool is_valid = box->CheckNumberInput(box->tag_input, box->tag_numbers);

	if (is_valid)
	{
		box->tag_input->textcolor(FL_FOREGROUND_COLOR);
		box->tag_input->redraw();
	}
	else
	{
		// uhhh, cannot disable all the search buttons [ too hard ]
		// so..... showing red is the all we can do....

		box->tag_input->textcolor(FL_RED);
		box->tag_input->redraw();
	}
}


bool UI_FindAndReplace::CheckNumberInput(Fl_Input *w, number_group_c *num_grp)
{
	num_grp->clear();

	// here empty string means match everything
	if (strlen(w->value()) == 0)
	{
		num_grp->ParseString("*");
		return true;
	}

	if (num_grp->ParseString(w->value()))
		return true;

	return false;
}


//------------------------------------------------------------------------


char UI_FindAndReplace::GetKind()
{
	// these letters are same as the Browser uses

	int v = what->value();

	if (v < 0 || v >= 5)
		return '?';

	const char *kinds = "OTFLS";

	return kinds[v];
}


void UI_FindAndReplace::BrowsedItem(char kind, int number, const char *name, int e_state)
{
	if (kind != GetKind())
	{
		fl_beep();
		return;
	}

	bool is_replace = false;

	if (Fl::focus() == rep_value || Fl::focus() == rep_desc)
		is_replace = true;

	char append = 0;

	// never append if user has selected some/all of the input
	if (! is_replace &&
		find_match->position() == find_match->mark())
	{
		append = ',';
	}

	// insert the chosen item

	Fl_Input *inp = is_replace ? rep_value : find_match;

	if (kind == 'T' || kind == 'F')
		InsertName(inp, append, name);
	else
	{
		// already present?
		if (! is_replace && find_numbers->get(number))
			return;

		InsertNumber(inp, append, number);
	}
}


void UI_FindAndReplace::InsertName(Fl_Input *inp, char append, const char *name)
{
	if (append)
	{
		int len = inp->size();

		// insert a separator, unless user has already put one there
		if (NeedSeparator(inp))
		{
			char buf[4];
			buf[0] = append;
			buf[1] = 0;

			inp->replace(len, len, buf);
			len += 1;
		}

		inp->replace(len, len, name);
	}
	else
	{
		inp->value(name);
	}

	inp->do_callback();

	Fl::focus(inp);
	inp->redraw();
}

void UI_FindAndReplace::InsertNumber(Fl_Input *inp, char append, int number)
{
	char buf[256];

	sprintf(buf, "%d", number);

	InsertName(inp, append, buf);
}


bool UI_FindAndReplace::NeedSeparator(Fl_Input *inp) const
{
	const char *str = inp->value();

	// nothing but whitespace?  --> no need
	while (isspace(*str))
		str++;

	if (str[0] == 0)
		return false;

	// ends with a punctuation symbol?  --> no need
	int p = (int)strlen(str) - 1;

	while (p >= 0 && isspace(str[p]))
		p--;

	if (p >= 0 && str[p] != '_' && ispunct(str[p]))
		return false;
	
	return true;
}


void UI_FindAndReplace::find_choose_callback(Fl_Widget *w, void *data)
{
	UI_FindAndReplace *box = (UI_FindAndReplace *)data;

	main_win->ShowBrowser(box->GetKind());

	// ensure Match input widget has the focus
	Fl::focus(box->find_match);
	box->find_match->redraw();
}

void UI_FindAndReplace::rep_choose_callback(Fl_Widget *w, void *data)
{
	UI_FindAndReplace *box = (UI_FindAndReplace *)data;

	main_win->ShowBrowser(box->GetKind());

	// ensure 'New val' input widget has the focus
	Fl::focus(box->rep_value);
	box->rep_value->redraw();
}


//------------------------------------------------------------------------


bool UI_FindAndReplace::FindNext()
{
	// this can happen via CTRL-G shortcut (View / Go to next)
	if (strlen(find_match->value()) == 0)
	{
		Beep("No find active!");
		return false;
	}

	ComputeFlagMask();

	if (cur_obj.type != edit.mode)
	{
		Editor_ChangeMode_Raw(cur_obj.type);

		// this clears the selection
		edit.Selected->change_type(edit.mode);
	}
	else
	{
		edit.Selected->clear_all();
	}

	edit.RedrawMap = 1;


	bool is_first = cur_obj.is_nil();

	int start_at = cur_obj.is_nil() ? 0 : (cur_obj.num + 1);
	int total    = NumObjects(cur_obj.type);

	for (int idx = start_at ; idx < total ; idx++)
	{
		if (MatchesObject(idx))
		{
			cur_obj.num = idx;

			if (is_first)
			{
				find_but->label("Next");
				rep_value->do_callback();
			}

			GoToObject(cur_obj);

			Status_Set("Found #%d", idx);
			return true;
		}
	}

	// nothing (else) was found

	cur_obj.clear();

	find_but->label("Find");
	rep_value->do_callback();

	if (is_first)
		Beep("Nothing found");
	else
		Beep("No more found");

	return false;
}


void UI_FindAndReplace::DoReplace()
{
	// sanity check  [ should never happen ]
	if (strlen(find_match->value()) == 0 ||
		strlen( rep_value->value()) == 0)
	{
		Beep("Bad replace");
		return;
	}

	// this generally can't happen either
	if (cur_obj.is_nil())
	{
		Beep("No object to replace");
		return;
	}

	BA_Begin();

	ApplyReplace(cur_obj.num);

	BA_End();

	// move onto next object
	FindNext();
}


bool UI_FindAndReplace::MatchesObject(int idx)
{
	switch (what->value())
	{
		case 0: // Things
			return Match_Thing(idx);

		case 1: // LineDefs (texturing)
			return Match_LineDef(idx);

		case 2: // Sectors (texturing)
			return Match_Sector(idx);

		case 3: // Lines by Type
			return Match_LineType(idx);

		case 4: // Sectors by Type
			return Match_SectorType(idx);

		default: return false;
	}
}


void UI_FindAndReplace::ApplyReplace(int idx)
{
	SYS_ASSERT(idx >= 0);

	switch (what->value())
	{
		case 0: // Things
			Replace_Thing(idx);
			break;

		case 1: // LineDefs (texturing)
			Replace_LineDef(idx);
			break;

		case 2: // Sectors (texturing)
			Replace_Sector(idx);
			break;

		case 3: // Lines by Type
			Replace_LineType(idx);
			break;

		case 4: // Sectors by Type
			Replace_SectorType(idx);
			break;

		default: break;
	}
}


void UI_FindAndReplace::DoAll(bool replace)
{
	if (strlen(find_match->value()) == 0)
	{
		Beep("No find active!");
		return;
	}

	ComputeFlagMask();

	if (cur_obj.type != edit.mode)
		Editor_ChangeMode_Raw(cur_obj.type);

	if (replace)
	{
		BA_Begin();
	}

	// we select objects even in REPLACE mode
	// (gives the user a visual indication that stuff was done)

	// this clears the selection
	edit.Selected->change_type(edit.mode);

	int total = NumObjects(cur_obj.type);
	int count = 0;

	for (int idx = 0 ; idx < total ; idx++)
	{
		if (! MatchesObject(idx))
			continue;

		count++;

		if (replace)
			ApplyReplace(idx);

		edit.Selected->set(idx);
	}

	if (count == 0)
		Beep("Nothing found");
	else
		Status_Set("Found %d objects", count);

	if (replace)
	{
		BA_End();
	}

	if (count > 0)
		GoToSelection();

	if (replace)
	{
		cur_obj.clear();
		rep_value->do_callback();

		edit.error_mode = true;
	}

	edit.RedrawMap = 1;
}


//------------------------------------------------------------------------
//    MATCHING METHODS
//------------------------------------------------------------------------

bool UI_FindAndReplace::Match_Thing(int idx)
{
	const Thing *T = Things[idx];

	if (! find_numbers->get(T->type))
		return false;

	// skill/mode flag filter
	if ((T->options & options_mask) != options_value)
		return false;

	return true;
}


bool UI_FindAndReplace::Match_LineDef(int idx)
{
	const LineDef *L = LineDefs[idx];

	if (! Filter_Tag(L->tag))
		return false;

	// TODO
	return false;
}


bool UI_FindAndReplace::Match_Sector(int idx)
{
	const Sector *SEC = Sectors[idx];

	if (! Filter_Tag(SEC->tag))
		return false;

	// TODO
	return false;
}


bool UI_FindAndReplace::Match_LineType(int idx)
{
	const LineDef *L = LineDefs[idx];

	if (! find_numbers->get(L->type))
		return false;

	if (! Filter_Tag(L->tag))
		return false;

	return true;
}


bool UI_FindAndReplace::Match_SectorType(int idx)
{
	const Sector *SEC = Sectors[idx];

	if (! find_numbers->get(SEC->type))
		return false;

	if (! Filter_Tag(SEC->tag))
		return false;

	return true;
}


bool UI_FindAndReplace::Filter_Tag(int tag)
{
	if (! filter_toggle->value())
		return true;
	
	return tag_numbers->get(tag);
}


void UI_FindAndReplace::ComputeFlagMask()
{
	options_mask  = 0;
	options_value = 0;

	if (! filter_toggle->value())
	{
		// this will always succeed
		return;
	}

#define FLAG_FROM_WIDGET(w, mul, flag)  \
	if ((w)->value() != 0)  \
	{  \
		options_mask |= (flag);  \
		if ((w)->value() * (mul) > 0)  \
			options_value |= (flag);  \
	}

	FLAG_FROM_WIDGET(  o_easy, 1, MTF_Easy);
	FLAG_FROM_WIDGET(o_medium, 1, MTF_Medium);
	FLAG_FROM_WIDGET(  o_hard, 1, MTF_Hard);

	if (game_info.coop_dm_flags)
	{
		FLAG_FROM_WIDGET(  o_sp, -1, MTF_Not_SP);
		FLAG_FROM_WIDGET(o_coop, -1, MTF_Not_COOP);
		FLAG_FROM_WIDGET(  o_dm, -1, MTF_Not_DM);
	}
	else	// vanilla DOOM
	{
		FLAG_FROM_WIDGET(o_dm, 1, MTF_Not_SP);
	}

#undef FLAG_FROM_WIDGET
}


//------------------------------------------------------------------------
//    REPLACE METHODS
//------------------------------------------------------------------------

void UI_FindAndReplace::Replace_Thing(int idx)
{
	int new_type = atoi(rep_value->value());

	BA_ChangeTH(idx, Thing::F_TYPE, new_type);
}


void UI_FindAndReplace::Replace_LineDef(int idx)
{
	// TODO
}


void UI_FindAndReplace::Replace_Sector(int idx)
{
	// TODO
}


void UI_FindAndReplace::Replace_LineType(int idx)
{
	int new_type = atoi(rep_value->value());

	BA_ChangeLD(idx, LineDef::F_TYPE, new_type);
}


void UI_FindAndReplace::Replace_SectorType(int idx)
{
	int new_type = atoi(rep_value->value());

	BA_ChangeSEC(idx, Sector::F_TYPE, new_type);
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
