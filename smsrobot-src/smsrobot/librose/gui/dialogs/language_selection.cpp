/* $Id: language_selection.cpp 48740 2011-03-05 10:01:34Z mordante $ */
/*
   Copyright (C) 2008 - 2011 by Mark de Wever <koraq@xs4all.nl>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#define GETTEXT_DOMAIN "rose-lib"

#include "gui/dialogs/language_selection.hpp"

#include "gui/widgets/listbox.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/window.hpp"
#include "language.hpp"
#include <preferences.hpp>
#include <hero.hpp>

#include <boost/foreach.hpp>

namespace gui2 {

/*WIKI
 * @page = GUIWindowDefinitionWML
 * @order = 2_language_selection
 *
 * == Language selection ==
 *
 * This shows the dialog to select the language to use. When the dialog is
 * closed with the OK button it also updates the selected language in the
 * preferences.
 *
 * @begin{table}{dialog_widgets}
 *
 * language_list & & listbox & m &
 *         This listbox contains the list with available languages. $
 *
 * - & & control & o &
 *         Show the name of the language in the current row. $
 *
 * @end{table}
 */

/**
 * @todo show we also reset the translations and is the tips of day call
 * really needed?
 */

REGISTER_DIALOG(rose, language_selection)

void tlanguage_selection::pre_show()
{
	window_->set_canvas_variable("border", variant("default-border"));

	tlistbox& list = find_widget<tlistbox>(window_, "language_list", false);
	window_->keyboard_capture(&list);

	const std::vector<language_def>& languages = get_languages();
	const language_def& current_language = get_language();
	std::stringstream strstr;
	int number = hero::number_normal_min;
	BOOST_FOREACH (const language_def& lang, languages) {
		std::map<std::string, std::string> list_item_item;

		strstr.str("");
		strstr << "hero-64/" << number << ".png~SCALE(64, 80)";
		list_item_item.insert(std::make_pair("icon", strstr.str()));

		list_item_item.insert(std::make_pair("name", lang.language));

		list.insert_row(list_item_item);

		if (lang == current_language) {
			list.select_row(list.rows() - 1);
		}
		number ++;
	}
}

void tlanguage_selection::post_show()
{
	if (get_retval() == twindow::OK) {
		const int res = find_widget<tlistbox>(window_, "language_list", false).cursel()->at();

		const std::vector<language_def>& languages = get_languages();
		::set_language(languages[res]);
		preferences::set_language(languages[res].localename);
	}
}

} // namespace gui2
