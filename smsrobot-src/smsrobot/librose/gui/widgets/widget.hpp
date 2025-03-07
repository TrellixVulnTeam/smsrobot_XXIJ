/* $Id: widget.hpp 54906 2012-07-29 19:52:01Z mordante $ */
/*
   Copyright (C) 2007 - 2012 by Mark de Wever <koraq@xs4all.nl>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#ifndef GUI_WIDGETS_WIDGET_HPP_INCLUDED
#define GUI_WIDGETS_WIDGET_HPP_INCLUDED

#include "gui/auxiliary/event/dispatcher.hpp"
#include "util.hpp"
#include "gui/widgets/event_executor.hpp"
#include "gui/widgets/helper.hpp"
#include "const_clone.tpp"
#include "gui/auxiliary/formula.hpp"
#include "integrate.hpp"
#include "wml_exception.hpp"

#include <boost/noncopyable.hpp>

#include <string>
#include <cassert>

namespace gui2 {

struct tbuilder_widget;
class tdialog;
class twindow;

// typedef std::map< std::string, t_string > string_map;
int cfg_2_os_size(const int cfg_size);
int os_size_2_cfg(const int os_size);

extern int SCROLLBAR_BEST_SIZE;
extern int SCROLLBAR2_BEST_SIZE;

/**
 * Base class for all widgets.
 *
 * From this abstract all other items should inherit. It contains the minimal
 * info needed for a real widget and some pure abstract functions which need to
 * be implemented by classes inheriting from this class.
 */
class twidget
	: private boost::noncopyable
	, public virtual tevent_executor
	, public event::tdispatcher
{
	friend class twindow; // needed for modifying the layout_size.

public:
	class tfire_event_widget_lock
	{
	public:
		tfire_event_widget_lock(const twidget& widget)
			: original_(fire_event)
		{
			fire_event = &widget;
		}
		~tfire_event_widget_lock()
		{
			fire_event = original_;
		}

	private:
		// fire_event maybe nest.
		const twidget* original_;
	};
	static const twidget* fire_event;

	class tlink_group_owner_lock
	{
	public:
		tlink_group_owner_lock(twidget& widget)
			: original_(link_group_owner)
		{
			link_group_owner = &widget;
		}
		~tlink_group_owner_lock()
		{
			link_group_owner = original_;
		}

	private:
		// linke group owner maybe nest.
		twidget* original_;
	};
	static twidget* link_group_owner;

	class tclear_restrict_width_cell_size_lock
	{
	public:
		tclear_restrict_width_cell_size_lock()
			: original_(clear_restrict_width_cell_size)
		{
			// false --> true or true --> true.
			clear_restrict_width_cell_size = true;
		}
		~tclear_restrict_width_cell_size_lock()
		{
			clear_restrict_width_cell_size = original_;
		}

	private:
		bool original_;
	};
	static bool clear_restrict_width_cell_size;

	class tdisable_invalidate_layout_lock
	{
	public:
		tdisable_invalidate_layout_lock()
			: original_(clear_restrict_width_cell_size)
		{
			// false --> true or true --> true.
			disalbe_invalidate_layout = true;
		}
		~tdisable_invalidate_layout_lock()
		{
			disalbe_invalidate_layout = original_;
		}

	private:
		bool original_;
	};
	static bool disalbe_invalidate_layout;

	enum torientation {auto_orientation, landscape_orientation, portrait_orientation};
	static bool current_landscape;
	static bool landscape_from_orientation(torientation orientation, bool def);
	static bool should_conside_orientation(const int width, const int height);
	static tpoint orientation_swap_size(int width, int height);

	static int hdpi_scale;
	static const int max_effectable_point;

	enum tdrag_direction { drag_none, drag_left = 0x1, drag_right = 0x2, drag_up = 0x4, drag_down = 0x8, drag_track = 0x10};
	enum tmouse_event {mouse_down, mouse_leave, mouse_motion};

	/** @deprecated use the second overload. */
	twidget();

	virtual ~twidget();

	/***** ***** ***** ***** flags ***** ***** ***** *****/
	
	/** Visibility settings done by the user. */
	enum tvisible {
		/**
		 * The user set the widget visible, that means:
		 * * widget is visible.
		 * * find_at always 'sees' the widget (the active flag is tested later).
		 * * widget (if active) handles events (and sends events to children)
		 * * widget is drawn (and sends populate_dirty_list to children)
		 */
		VISIBLE,
		/**
		 * The user set the widget hidden, that means:
		 * * item is invisible but keeps its size.
		 * * find_at 'sees' the widget if active is false.
		 * * item doesn't handle events (and doesn't send events to children).
		 * * item doesn't populate_dirty_list (nor does it send the request
		 *   to its children).
		 */
		HIDDEN,
		/**
		 * The user set the widget invisible, that means:
		 * * item is invisible and gridcell has size 0,0.
		 * * find_at never 'sees' the widget.
		 * * item doesn't handle events (and doesn't send events to children).
		 * * item doesn't populate_dirty_list (nor does it send the request
		 *   to its children).
		 */
		INVISIBLE };

	/**
	 * Visibility set by the engine.
	 *
	 * This state only will be used if the widget is visible, depending on
	 * this state the widget might not be visible after all.
	 */
	enum tdrawing_action {
		/** The widget is fully visible and should redrawn when set dirty. */
		DRAWN,
		/**
		 * The widget is partly visible, in order to render it, it's clip
		 * rect needs to be used.
		 */
		PARTLY_DRAWN,
		/** The widget is not visible and should not be drawn if dirty. */
		NOT_DRAWN
	};

	class tvisible_lock
	{
	public:
		tvisible_lock(twidget& widget, tvisible into)
			: widget_(widget)
			, into_(into)
		{
			widget.set_visible(into);
		}
		~tvisible_lock()
		{
			widget_.set_visible(VISIBLE);
		}

	private:
		twidget& widget_;
		tvisible into_;
	};

	/***** ***** ***** ***** layout functions ***** ***** ***** *****/

	/**
	 * How the layout engine works.
	 *
	 * Every widget has a member layout_size_ which holds the best size in
	 * the current layout phase. When the windows starts the layout phase it
	 * calls layout_init() which resets this value.
	 *
	 * Every widget has two function to get the best size. get_best_size() tests
	 * whether layout_size_ is set and if so returns that value otherwise it
	 * calls calculate_best_size() so the size can be updated.
	 *
	 * During the layout phase some functions can modify layout_size_ so the
	 * next call to get_best_size() returns the currently best size. This means
	 * that after the layout phase get_best_size() still returns this value.
	 */

	/**
	 * Initializes the layout phase.
	 *
	 * Clears the initial best size for the widgets.
	 *
	 * @see @ref layout_algorithm for more information.
	 *
	 */
	virtual void layout_init(bool linked_group_only);

	/**
	 * Gets the best size for the widget.
	 *
	 * During the layout phase a best size will be determined, several stages
	 * might change the best size. This function will return the currently best
	 * size as determined during the layout phase.
	 *
	 * @returns                      The best size for the widget.
	 * @retval 0,0                   The best size is 0,0.
	 */
	tpoint get_best_size() const;

private:
	/**
	 * Calculates the best size.
	 *
	 * This function calculates the best size and ignores the current values in
	 * the layout phase. Note containers can call the get_best_size() of their
	 * children since it's meant to update itself.
	 *
	 * @returns                      The best size for the widget.
	 * @retval 0,0                   The best size is 0,0.
	 */
	virtual tpoint calculate_best_size() const = 0;

public:
	virtual tpoint calculate_best_size_bh(const int width) { return tpoint(nposm, nposm); }
	/**
	 * Places the widget.
	 *
	 * This function is normally called by a layout function to do the
	 * placement of a widget.
	 *
	 * @param origin              The position of top left of the widget.
	 * @param size                The size of the widget.
	 */
	virtual void place(const tpoint& origin, const tpoint& size);

	/**
	 * Sets the size of the widget.
	 *
	 * This version is meant to resize a widget, since the origin isn't
	 * modified. This can be used if a widget needs to change its size and the
	 * layout will be fixed later.
	 *
	 * @param size                The size of the widget.
	 */
	virtual void set_size(const tpoint& size);

	void set_width(const int width);

	virtual void dirty_under_rect(const SDL_Rect& clip);

	/***** ***** ***** ***** query ***** ***** ***** *****/

	/**
	 * Gets the widget at the wanted coordinates.
	 *
	 * @param coordinate          The coordinate which should be inside the
	 *                            widget.
	 * @param must_be_active      The widget should be active, not all widgets
	 *                            have an active flag, those who don't ignore
	 *                            flag.
	 *
	 * @returns                   The widget with the id.
	 * @retval 0                  No widget at the wanted coordinate found (or
	 *                            not active if must_be_active was set).
	 */
	virtual twidget* find_at(const tpoint& coordinate,
			const bool must_be_active);

	/**
	 * Gets a widget with the wanted id.
	 *
	 * @param id                  The id of the widget to find.
	 * @param must_be_active      The widget should be active, not all widgets
	 *                            have an active flag, those who don't ignore
	 *                            flag.
	 *
	 * @returns                   The widget with the id.
	 * @retval 0                  No widget with the id found (or not active if
	 *                            must_be_active was set).
	 */
	virtual twidget* find(const std::string& id,
			const bool /*must_be_active*/)
		{ return id_ == id ? this : 0; }

	/** The const version of find. */
	virtual const twidget* find(const std::string& id,
			const bool /*must_be_active*/) const
		{ return id_ == id ? this : 0; }

	/**
	 * Does the widget contain the widget.
	 *
	 * Widgets can be containers which have more widgets inside them, this
	 * function will traverse in those child widgets and tries to find the
	 * wanted widget.
	 *
	 * @param widget              Pointer to the widget to find.
	 * @returns                   Whether or not the widget was found.
	 */
	virtual bool has_widget(const twidget* widget) const
		{ return widget == this; }

	/***** ***** ***** ***** query parents ***** ***** ***** *****/

	/**
	 * Get the parent window.
	 *
	 * @returns                   Pointer to parent window.
	 * @retval 0                  No parent window found.
	 */
	twindow* get_window();

	/** The const version of get_window(). */
	const twindow* get_window() const;

	/**
	 * Returns the toplevel dialog.
	 *
	 * A window is most of the time created by a dialog, this function returns
	 * that dialog.
	 *
	 * @deprecated The function was used to install callbacks to member
	 * functions of the dialog. Once all widgets are converted to signals this
	 * function will be removed.
	 *
	 * @returns                   The toplevel dialog.
	 * @retval 0                  No toplevel window or the toplevel window is
	 *                            not owned by a dialog.
	 */
	tdialog* dialog();

	/***** ***** ***** Misc. ***** ****** *****/

	/** Does the widget disable easy close? */
	virtual bool disable_click_dismiss() const = 0;

	/***** ***** ***** setters / getters for members ***** ****** *****/

	twidget* parent() const { return parent_; }
	void set_parent(twidget* parent) { parent_ = parent; }

	const std::string& id() const { return id_; }
	void set_id(const std::string& id);

	int get_width() const { return w_; }
	int get_height() const { return h_; }

	/** Returns the size of the widget. */
	tpoint get_size() const { return tpoint(w_, h_); }

	/** Returns the screen origin of the widget. */
	tpoint get_origin() const { return tpoint(x_, y_); }

	/** Gets the sizes in one rect structure. */
	SDL_Rect get_rect() const { return create_rect(get_origin(), get_size()); }

	virtual SDL_Rect get_float_widget_ref_rect() const { return create_rect(get_origin(), get_size()); }

	/**
	 * Gets the dirty rect of the widget.
	 *
	 * Depending on the drawing action it returns the rect this widget dirties
	 * while redrawing.
	 *
	 * @returns                   The dirty rect.
	 */
	SDL_Rect get_dirty_rect() const;

	/**
	 * Sets the origin of the widget.
	 *
	 * This function can be used to move the widget without dirting it.
	 *
	 * @param origin              The new origin.
	 */
	virtual void set_origin(const tpoint& origin);

	/**
	 * Sets the visible area for a widget.
	 *
	 * This function sets the drawing_state_ and the clip_rect_.
	 *
	 * @param area                The visible area in screen coordinates.
	 */
	virtual void set_visible_area(const SDL_Rect& area);

	void set_cookie(size_t cookie) { cookie_ = cookie; }
	size_t cookie() const { return cookie_; }

	/**
	 * Moves a widget.
	 *
	 * This function can be used to move the widget without dirtying it.
	 *
	 * @todo Implement the function to all inherited classes.
	 *
	 * @param x_offset            The amount of pixels to move the widget in
	 *                            the x direction.
	 * @param y_offset            The amount of pixels to move the widget in
	 *                            the y direction.
	 */
	virtual void move(const int x_offset, const int y_offset);

	int get_x() const { return x_; }

	int get_y() const { return y_; }

	void set_visible(const tvisible visible);
	tvisible get_visible() const { return visible_; }

	tdrawing_action get_drawing_action() const;

	/**
	 * Sets the widgets dirty state.
	 *
	 * @todo test whether nobody redefines the function.
	 */
	void set_dirty();
	bool get_dirty() const { return dirty_; }

	/**
	 * Sets the widgets redraw state. lighweight than dirty.
	 *
	 * @todo test whether nobody redefines the function.
	 */
	bool get_redraw() const { return redraw_; }

	void clear_dirty();

	/**
	 * Calculates the blitting rectangle of the widget.
	 *
	 * The blitting rectangle is to entire widget rectangle, but offsetted for
	 * drawing position.
	 *
	 * @param x_offset            The x offset when drawn.
	 * @param y_offset            The y offset when drawn.
	 *
	 * @returns                   The drawing rectangle.
	 */
	SDL_Rect calculate_blitting_rectangle(
			  const int x_offset
			, const int y_offset);

	/**
	 * Calculates the clipping rectangle of the widget.
	 *
	 * The clipping rectangle is used then the @ref drawing_action_ is
	 * @ref PARTLY_DRAWN. Since the drawing can be offsetted it also needs
	 * offset paramters.
	 *
	 * @param x_offset            The x offset when drawn.
	 * @param y_offset            The y offset when drawn.
	 *
	 * @returns                   The clipping rectangle.
	 */
	SDL_Rect calculate_clipping_rectangle(const int x_offset, const int y_offset);

	/**
	 * Draws the background of a widget.
	 *
	 * Subclasses should override impl_draw_background instead of changing
	 * this function.
	 *
	 * @param frame_buffer        The surface to draw upon.
	 * @param x_offset            The x offset in the @p frame_buffer to draw.
	 * @param y_offset            The y offset in the @p frame_buffer to draw.
	 */
	void draw_background(texture& frame_buffer, int x_offset, int y_offset);

	/**
	 * Draws the children of a widget.
	 *
	 * Containers should draw their children when they get this request.
	 *
	 * Subclasses should override impl_draw_children instead of changing
	 * this function.
	 *
	 * @param frame_buffer        The surface to draw upon.
	 * @param x_offset            The x offset in the @p frame_buffer to draw.
	 * @param y_offset            The y offset in the @p frame_buffer to draw.
	 */
	void draw_children(texture& frame_buffer, int x_offset, int y_offset);

	/**
	 * Allows a widget to update its children.
	 *
	 * Before the window is populating the dirty list the widgets can update
	 * their content, which allows delayed initialization. This delayed
	 * initialization is only allowed if the widget resizes itself, not when
	 * being placed.
	 */
	virtual void layout_children() {}

	/**
	 * Adds a widget to the dirty list if it is dirty.
	 *
	 * See twindow::dirty_list_ for more info on the dirty list.
	 *
	 * If the widget is not dirty and has children it should add itself to the
	 * call_stack and call child_populate_dirty_list with the new call_stack.
	 *
	 * @param caller              The parent window, if dirty it should
	 *                            register itself to this window.
	 * @param call_stack          The callstack of widgets traversed to reach
	 *                            this function.
	 */
	void populate_dirty_list(twindow& caller,
			std::vector<twidget*>& call_stack);

	virtual bool popup_new_window() { return false; }

	virtual void broadcast_frame_buffer(texture& frame_buffer) {}
	virtual void clear_texture() {}

	virtual bool exist_anim() { return false; }

	/***** ***** ***** setters / getters for members ***** ****** *****/
	void set_layout_size(const tpoint& size);
	const tpoint& layout_size() const { return layout_size_; }

	bool float_widget() const { return float_widget_; }
	bool restrict_width() const { return restrict_width_; }

	virtual bool captured_mouse_can_to_scroll_container() const { return false; }
	virtual bool captured_keyboard_can_to_scroll_container() const { return true; }
	virtual bool is_text_box() const { return false; }
	virtual bool is_scroll_container() const { return false; }

	bool allow_handle_event() const { return allow_handle_event_; }
	void disable_allow_handle_event() { allow_handle_event_ = false; }

	bool can_invalidate_layout() const { return can_invalidate_layout_; }
	virtual void invalidate_layout(twidget* widget) {}
	void trigger_invalidate_layout(twindow* window);

private:

	virtual void validate_visible_pre_change(const twidget& widget) const {}

	/**
	 * Tries to add all children of a container to the dirty list.
	 *
	 * @note The function is private since everybody should call
	 * populate_dirty_list instead.
	 *
	 * @param caller              The parent window, if dirty it should
	 *                            register itself to this window.
	 * @param call_stack          The callstack of widgets traversed to reach
	 *                            this function.
	 */
	virtual void child_populate_dirty_list(twindow& caller, const std::vector<twidget*>& call_stack) {}
	virtual void add_linked_widget(const std::string& id, twidget& widget, bool linked_group_only) {}

	virtual SDL_Rect get_draw_background_rect() const { return ::create_rect(x_, y_, w_, h_); }

public:

	void set_linked_group(const std::string& linked_group)
	{
		linked_group_ = linked_group;
	}

	/**
	 * Returns the control_type of the control.
	 *
	 * The control_type parameter for tgui_definition::get_control() To keep the
	 * code more generic this type is required so the controls need to return
	 * the proper string here.  Might be used at other parts as well the get the
	 * type of
	 * control involved.
	 */
	virtual const std::string& get_control_type() const = 0;

protected:
	/**
	 * The id is the unique name of the widget in a certain context.
	 *
	 * This is needed for certain widgets so the engine knows which widget is
	 * which. Eg it knows which button is pressed and thus which engine action
	 * is connected to the button. This doesn't mean that the id is unique in a
	 * window, eg a listbox can have the same id for every row.
	 */
	std::string id_;

	/** The x coordinate of the widget in the screen. */
	int x_;

	/** The y coordinate of the widget in the screen. */
	int y_;

	/** The width of the widget. */
	int w_;

	/** The height of the widget. */
	int h_;

	/**
	 * The parent widget, if the widget has a parent it contains a pointer to
	 * the parent, else it's set to NULL.
	 */
	twidget* parent_;

	// cookie_ maybe cast to void*. On win64, sizeof(long) is 4! so must not use long.
	size_t cookie_;

	/** Field for the status of the visibility. */
	tvisible visible_;

	/**
	 * always require refresh.
	 */
	bool redraw_;

	bool float_widget_;

	/**
	 * The best size for the control.
	 *
	 * When 0,0 the real best size is returned, but in the layout phase a
	 * wrapping or a scrollbar might change the best size for that widget.
	 * This variable holds that best value.
	 */
	tpoint layout_size_;

	bool restrict_width_;

	bool terminal_;
	bool allow_handle_event_;
	bool can_invalidate_layout_;

private:
	/**
	 * Is the widget dirty? When a widget is dirty it needs to be redrawn at
	 * the next drawing cycle, setting it to dirty also need to set it's parent
	 * dirty so at so point the toplevel parent knows which item to redraw.
	 *
	 * NOTE dirtying the parent might be inefficient and this behaviour might be
	 * optimized later on.
	 */
	bool dirty_;

	/** Field for the action to do on a drawing request. */
	tdrawing_action drawing_action_;

	/** The clip rect is a widget is partly visible. */
	SDL_Rect clip_rect_;

	/**
	 * The linked group the widget belongs to.
	 *
	 * @todo For now the linked group is initialized when the layout of the
	 * widget is initialized. The best time to set it would be upon adding the
	 * widget in the window. Need to look whether it's possible in a clean way.
	 * Maybe a signal just prior to showing a window where the widget can do
	 * some of it's on things, would also be nice for widgets that need a
	 * finalizer function.
	 */
	std::string linked_group_;

	/** See draw_background(). */
	virtual void impl_draw_background(
			  texture& /*frame_buffer*/
			, int /*x_offset*/
			, int /*y_offset*/)
	{
	}

	/** See draw_children. */
	virtual void impl_draw_children(
			texture& /*frame_buffer*/
			, int /*x_offset*/
			, int /*y_offset*/)
	{
	}

	/** (Will be) inherited from event::tdispatcher. */
	virtual bool is_at(const tpoint& coordinate) const
	{
		return is_at(coordinate, true);
	}

	/**
	 * Is the coordinate inside our area.
	 *
	 * Helper for find_at so also looks at our visibility.
	 *
	 * @param coordinate          The coordinate which should be inside the
	 *                            widget.
	 * @param must_be_active      The widget should be active, not all widgets
	 *                            have an active flag, those who don't ignore
	 *                            flag.
	 *
	 * @returns                   Status.
	 */
	bool is_at(const tpoint& coordinate, const bool must_be_active) const;
};

class tbase_tpl_widget: private boost::noncopyable
{
public:
	tbase_tpl_widget(twindow& window, twidget& widget)
		: window_(window)
		, widget2_(widget)
	{}

protected:
	twindow& window_;
	twidget& widget2_;
};

struct tformula_blit: public image::tblit
{
	// in normal, think below value must not be appear at the same time for x, y, width, and height.
	enum {SURF_RATIO_CENTER = 100000, SURF_RATIO_LT};

	tformula_blit(const surface& surf, int _x, int _y, int _width, int _height, const SDL_Rect& _clip = SDL_Rect())
		: tblit(surf, _x, _y, _width, _height, _clip)
		, formula_x(null_str)
		, formula_y(null_str)
		, formula_width(null_str)
		, formula_height(null_str)
	{}
	tformula_blit(const image::locator& loc, const std::string& x, const std::string& y, const std::string& width, const std::string& height, const SDL_Rect& _clip = SDL_Rect())
		: tblit(loc, image::UNSCALED, MAGIC_COORDINATE_X, MAGIC_COORDINATE_X, MAGIC_COORDINATE_X, MAGIC_COORDINATE_X, _clip)
		, formula_x(x)
		, formula_y(y)
		, formula_width(width)
		, formula_height(height)
	{}

	tformula_blit(const image::locator& loc, int _x, int _y, int _width, int _height, const SDL_Rect& _clip = SDL_Rect())
		: tblit(loc, image::UNSCALED, _x, _y, _width, _height, _clip)
		, formula_x(null_str)
		, formula_y(null_str)
		, formula_width(null_str)
		, formula_height(null_str)
	{}
	tformula_blit(const surface& surf, const std::string& x, const std::string& y, const std::string& width, const std::string& height, const SDL_Rect& _clip = SDL_Rect())
		: tblit(surf, MAGIC_COORDINATE_X, MAGIC_COORDINATE_X, MAGIC_COORDINATE_X, MAGIC_COORDINATE_X, _clip)
		, formula_x(x)
		, formula_y(y)
		, formula_width(width)
		, formula_height(height)
	{}

	tformula_blit(const int type, const int x, const int y, const int width, const int height, const uint32_t color)
		: tblit(type, x, y, width, height, color)
		, formula_x(null_str)
		, formula_y(null_str)
		, formula_width(null_str)
		, formula_height(null_str)
	{}
	tformula_blit(int type, const std::string& x, const std::string& y, const std::string& width, const std::string& height, uint32_t color)
		: tblit(type, MAGIC_COORDINATE_X, MAGIC_COORDINATE_X, MAGIC_COORDINATE_X, MAGIC_COORDINATE_X, color)
		, formula_x(x)
		, formula_y(y)
		, formula_width(width)
		, formula_height(height)
	{}

	tformula<int> formula_x;
	tformula<int> formula_y;
	tformula<int> formula_width;
	tformula<int> formula_height;
};

void blit_integer_blits(std::vector<tformula_blit>& blits, const int canvas_width, const int canvas_height, const int x, const int y, int integer);
void generate_integer2_blits(std::vector<tformula_blit>& blits, int width, int height, const std::string& img, int integer, bool greyscale);
void generate_pip_blits(std::vector<tformula_blit>& blits, const std::string& bg, const std::string& fg);
void generate_pip_blits2(std::vector<tformula_blit>& blits, int width, int height, const std::string& bg, const std::string& fg);

/**
 * Returns the first parent of a widget with a certain type.
 *
 * @param widget                  The widget to get the parent from,
 * @tparam T                      The class of the widget to return.
 *
 * @returns                       The parent widget.
 */
template<class T> T* get_parent(twidget* widget)
{
	T* result;
	do {
		widget = widget->parent();
		result = dynamic_cast<T*>(widget);

	} while (widget && !result);

	assert(result);
	return result;
}

/**
 * Gets a widget with the wanted id.
 *
 * This template function doesn't return a pointer to a generic widget but
 * returns the wanted type and tests for its existence if required.
 *
 * @param widget              The widget test or find a child with the wanted
 *                            id.
 * @param id                  The id of the widget to find.
 * @param must_be_active      The widget should be active, not all widgets
 *                            have an active flag, those who don't ignore
 *                            flag.
 * @param must_exist          The widget should be exist, the function will
 *                            fail if the widget doesn't exist or is
 *                            inactive and must be active. Upon failure a
 *                            wml_error is thrown.
 *
 * @returns                   The widget with the id.
 */
template<class T>
T* find_widget(typename utils::tconst_clone<twidget, T>::pointer widget
		, const std::string& id
		, const bool must_be_active
		, const bool must_exist)
{
	T* result =
		dynamic_cast<T*>(widget->find(id, must_be_active));
	VALIDATE(!must_exist || result, missing_widget(id));

	return result;
}

/**
 * Gets a widget with the wanted id.
 *
 * This template function doesn't return a reference to a generic widget but
 * returns a reference to the wanted type
 *
 * @param widget              The widget test or find a child with the wanted
 *                            id.
 * @param id                  The id of the widget to find.
 * @param must_be_active      The widget should be active, not all widgets
 *                            have an active flag, those who don't ignore
 *                            flag.
 *
 * @returns                   The widget with the id.
 */
template<class T>
T& find_widget(typename utils::tconst_clone<twidget, T>::pointer widget
		, const std::string& id
		, const bool must_be_active)
{
	return *find_widget<T>(widget, id, must_be_active, true);
}

} // namespace gui2

/**
 * Registers a widget.
 *
 * Call this function to register a widget. Use this macro in the
 * implementation, inside the gui2 namespace.
 *
 * @see @ref gui2::load_widget_definitions for more information.
 *
 * @note When the type is tfoo_definition, the id "foo" and no special key best
 * use RESISTER_WIDGET(foo) instead.
 *
 * @param type                    Class type of the window to register.
 * @param id                      Id of the widget
 * @param key                     The id to load if differs from id.
 */
#define REGISTER_WIDGET3(                                                  \
		  type                                                             \
		, id                                                               \
		, key)                                                             \
namespace {                                                                \
                                                                           \
	namespace ns_##type {                                                  \
                                                                           \
		struct tregister_helper {                                          \
			tregister_helper()                                             \
			{                                                              \
				register_widget(#id, boost::bind(                          \
						  load_widget_definitions<type>                    \
						, _1                                               \
						, _2                                               \
						, _3                                               \
						, key));                                           \
                                                                           \
				register_builder_widget(#id, boost::bind(                  \
							  build_widget<implementation::tbuilder_##id>  \
							, _1));                                        \
			}                                                              \
		};                                                                 \
                                                                           \
		static tregister_helper register_helper;                           \
	}                                                                      \
}

/**
 * Wrapper for REGISTER_WIDGET3.
 *
 * "Calls" REGISTER_WIDGET3(tid_definition, id, _4)
 */
#define REGISTER_WIDGET(id) REGISTER_WIDGET3(t##id##_definition, id, _4)

#endif

