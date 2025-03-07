/* $Id: vertical_scrollbar.cpp 52533 2012-01-07 02:35:17Z shadowmaster $ */
/*
   Copyright (C) 2008 - 2012 by Mark de Wever <koraq@xs4all.nl>
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

#include "gui/auxiliary/window_builder/vertical_scrollbar.hpp"

#include "config.hpp"
#include "gui/widgets/vertical_scrollbar.hpp"

namespace gui2 {

namespace implementation {

tbuilder_vertical_scrollbar::tbuilder_vertical_scrollbar(const config& cfg)
	: tbuilder_control(cfg)
	, unrestricted_drag(cfg["unrestricted_drag"].to_bool())
{
}

twidget* tbuilder_vertical_scrollbar::build() const
{
	tvertical_scrollbar *widget = new tvertical_scrollbar(unrestricted_drag);

	init_control(widget);

	// Window builder: placed vertical scrollbar 'id' with definition 'definition'.

	return widget;
}

} // namespace implementation

} // namespace gui2

/*WIKI
 * @page = GUIWidgetInstanceWML
 * @order = 2_vertical_scrollbar
 *
 * == Vertical scrollbar ==
 *
 *
 * @begin{parent}{name="gui/window/resolution/grid/row/column/"}
 * @begin{tag}{name="vertical_scrollbar"}{min=0}{max=1}{super="generic/widget_instance"}
 * @end{tag}{name="vertical_scrollbar"}
 * @end{parent}{name="gui/window/resolution/grid/row/column/"}
 */

