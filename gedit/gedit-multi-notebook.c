/*
 * gedit-notebook-multi.c
 * This file is part of gedit
 *
 * Copyright (C) 2010 - Ignacio Casal Quinteiro
 *
 * gedit is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * gedit is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with gedit; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, 
 * Boston, MA  02110-1301  USA
 */


#include "gedit-multi-notebook.h"
#include "gedit-marshal.h"


#define GEDIT_MULTI_NOTEBOOK_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE((object), GEDIT_TYPE_MULTI_NOTEBOOK, GeditMultiNotebookPrivate))

struct _GeditMultiNotebookPrivate
{
	GtkWidget *active_notebook;
	GList     *notebooks;
	gint       total_tabs;

	GeditTab  *active_tab;

	guint      removing_notebook : 1;
};

enum
{
	PROP_0,
	PROP_ACTIVE_NOTEBOOK,
	PROP_ACTIVE_TAB
};

/* Signals */
enum
{
	NOTEBOOK_ADDED,
	NOTEBOOK_REMOVED,
	TAB_ADDED,
	TAB_REMOVED,
	TAB_CLOSE_REQUEST,
	TAB_DETACHED,
	TABS_REORDERED,
	SHOW_POPUP_MENU,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (GeditMultiNotebook, gedit_multi_notebook, GTK_TYPE_VBOX)

static void	remove_notebook		(GeditMultiNotebook *multi,
					 GtkWidget          *notebook);

static void
gedit_multi_notebook_get_property (GObject    *object,
				   guint       prop_id,
				   GValue     *value,
				   GParamSpec *pspec)
{
	GeditMultiNotebook *multi = GEDIT_MULTI_NOTEBOOK (object);

	switch (prop_id)
	{
		case PROP_ACTIVE_NOTEBOOK:
			g_value_set_object (value,
					    multi->priv->active_notebook);
			break;
		case PROP_ACTIVE_TAB:
			g_value_set_object (value,
					    multi->priv->active_tab);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
gedit_multi_notebook_finalize (GObject *object)
{
	GeditMultiNotebook *multi = GEDIT_MULTI_NOTEBOOK (object);

	g_list_free (multi->priv->notebooks);

	G_OBJECT_CLASS (gedit_multi_notebook_parent_class)->finalize (object);
}

static void
gedit_multi_notebook_class_init (GeditMultiNotebookClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = gedit_multi_notebook_finalize;
	object_class->get_property = gedit_multi_notebook_get_property;

	g_type_class_add_private (object_class, sizeof (GeditMultiNotebookPrivate));

	signals[NOTEBOOK_ADDED] =
		g_signal_new ("notebook-added",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (GeditMultiNotebookClass, notebook_added),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__OBJECT,
			      G_TYPE_NONE,
			      1,
			      GEDIT_TYPE_NOTEBOOK);
	signals[NOTEBOOK_REMOVED] =
		g_signal_new ("notebook-removed",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (GeditMultiNotebookClass, notebook_removed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__OBJECT,
			      G_TYPE_NONE,
			      1,
			      GEDIT_TYPE_NOTEBOOK);
	signals[TAB_ADDED] =
		g_signal_new ("tab-added",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (GeditMultiNotebookClass, tab_added),
			      NULL, NULL,
			      gedit_marshal_VOID__OBJECT_OBJECT,
			      G_TYPE_NONE,
			      2,
			      GEDIT_TYPE_NOTEBOOK,
			      GEDIT_TYPE_TAB);
	signals[TAB_REMOVED] =
		g_signal_new ("tab-removed",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (GeditMultiNotebookClass, tab_removed),
			      NULL, NULL,
			      gedit_marshal_VOID__OBJECT_OBJECT,
			      G_TYPE_NONE,
			      2,
			      GEDIT_TYPE_NOTEBOOK,
			      GEDIT_TYPE_TAB);
	signals[TAB_CLOSE_REQUEST] =
		g_signal_new ("tab-close-request",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (GeditMultiNotebookClass, tab_close_request),
			      NULL, NULL,
			      gedit_marshal_VOID__OBJECT_OBJECT,
			      G_TYPE_NONE,
			      2,
			      GEDIT_TYPE_NOTEBOOK,
			      GEDIT_TYPE_TAB);
	signals[TAB_DETACHED] =
		g_signal_new ("tab-detached",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (GeditMultiNotebookClass, tab_detached),
			      NULL, NULL,
			      gedit_marshal_VOID__OBJECT_OBJECT,
			      G_TYPE_NONE,
			      2,
			      GEDIT_TYPE_NOTEBOOK,
			      GEDIT_TYPE_TAB);
	signals[TABS_REORDERED] =
		g_signal_new ("tabs-reordered",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (GeditMultiNotebookClass, tabs_reordered),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE,
			      0);
	signals[SHOW_POPUP_MENU] =
		g_signal_new ("show-popup-menu",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (GeditMultiNotebookClass, show_popup_menu),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__BOXED,
			      G_TYPE_NONE,
			      1,
			      GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);

	g_object_class_install_property (object_class,
					 PROP_ACTIVE_NOTEBOOK,
					 g_param_spec_object ("active-notebook",
							      "Active Notebook",
							      "The Active Notebook",
							      GEDIT_TYPE_NOTEBOOK,
							      G_PARAM_READABLE |
							      G_PARAM_STATIC_STRINGS));
	g_object_class_install_property (object_class,
					 PROP_ACTIVE_TAB,
					 g_param_spec_object ("active-tab",
							      "Active Tab",
							      "The Active Tab",
							      GEDIT_TYPE_TAB,
							      G_PARAM_READABLE |
							      G_PARAM_STATIC_STRINGS));
}

static gboolean
notebook_button_press_event (GtkNotebook        *notebook,
			     GdkEventButton     *event,
			     GeditMultiNotebook *multi)
{
	if (GDK_BUTTON_PRESS == event->type && 3 == event->button)
	{
		g_signal_emit (G_OBJECT (multi), signals[SHOW_POPUP_MENU], 0,
			       event);

		return TRUE;
	}

	return FALSE;
}

static void 
notebook_tab_close_request (GeditNotebook      *notebook,
			    GeditTab           *tab,
			    GeditMultiNotebook *multi)
{
	g_signal_emit (G_OBJECT (multi), signals[TAB_CLOSE_REQUEST], 0,
		       notebook, tab);
}

static void
notebook_tab_detached (GeditNotebook      *notebook,
		       GeditTab           *tab,
		       GeditMultiNotebook *multi)
{
	g_signal_emit (G_OBJECT (multi), signals[TAB_DETACHED], 0,
		       notebook, tab);
}

static void
notebook_tabs_reordered (GeditNotebook      *notebook,
			 GeditMultiNotebook *multi)
{
	g_signal_emit (G_OBJECT (multi), signals[TABS_REORDERED], 0);
}

static void
notebook_page_removed (GtkNotebook        *notebook,
		       GtkWidget          *child,
		       guint               page_num,
		       GeditMultiNotebook *multi)
{
	GeditTab *tab = GEDIT_TAB (child);
	guint num_tabs;
	gboolean last_notebook;

	--multi->priv->total_tabs;
	num_tabs = gtk_notebook_get_n_pages (notebook);
	last_notebook = (multi->priv->notebooks->next == NULL);

	if (multi->priv->total_tabs == 0)
	{
		multi->priv->active_tab = NULL;

		g_object_notify (G_OBJECT (multi), "active-tab");
	}

	/* Not last notebook but last tab of the notebook, this means we have
	   to remove the current notebook */
	if (num_tabs == 0 && !multi->priv->removing_notebook &&
	    !last_notebook)
	{
		remove_notebook (multi, GTK_WIDGET (notebook));
	}

	g_signal_emit (G_OBJECT (multi), signals[TAB_REMOVED], 0, notebook, tab);
}

static void
notebook_page_added (GtkNotebook        *notebook,
		     GtkWidget          *child,
		     guint               page_num,
		     GeditMultiNotebook *multi)
{
	GeditTab *tab = GEDIT_TAB (child);

	++multi->priv->total_tabs;

	g_signal_emit (G_OBJECT (multi), signals[TAB_ADDED], 0, notebook, tab);
}

static void 
notebook_switch_page (GtkNotebook        *book,
		      GtkNotebookPage    *pg,
		      gint                page_num,
		      GeditMultiNotebook *multi)
{
	GeditTab *tab;

	/* CHECK: I don't know why but it seems notebook_switch_page is called
	two times every time the user change the active tab */
	tab = GEDIT_TAB (gtk_notebook_get_nth_page (book, page_num));
	if (tab != multi->priv->active_tab)
	{
		/* set the active tab */
		multi->priv->active_tab = tab;

		g_object_notify (G_OBJECT (multi), "active-tab");
	}
}

/* We need to figure out if the any of the internal widget of the notebook
   has got the focus to set the active notebook */
static void
notebook_set_focus (GtkContainer       *container,
		    GtkWidget          *widget,
		    GeditMultiNotebook *multi)
{
	if (GEDIT_IS_NOTEBOOK (container) &&
	    GTK_WIDGET (container) != multi->priv->active_notebook)
	{
		gint page_num;

		multi->priv->active_notebook = GTK_WIDGET (container);

		page_num = gtk_notebook_get_current_page (GTK_NOTEBOOK (container));
		notebook_switch_page (GTK_NOTEBOOK (container), NULL,
				      page_num, multi);

		g_object_notify (G_OBJECT (multi), "active-notebook");
	}
}

static void
connect_notebook_signals (GeditMultiNotebook *multi,
			  GtkWidget          *notebook)
{
	g_signal_connect (notebook,
			  "set-focus-child",
			  G_CALLBACK (notebook_set_focus),
			  multi);
	g_signal_connect (notebook,
			  "page-added",
			  G_CALLBACK (notebook_page_added),
			  multi);
	g_signal_connect (notebook,
			  "page-removed",
			  G_CALLBACK (notebook_page_removed),
			  multi);
	g_signal_connect (notebook,
			  "switch-page",
			  G_CALLBACK (notebook_switch_page),
			  multi);
	g_signal_connect (notebook,
			  "tabs-reordered",
			  G_CALLBACK (notebook_tabs_reordered),
			  multi);
	g_signal_connect (notebook,
			  "tab-detached",
			  G_CALLBACK (notebook_tab_detached),
			  multi);
	g_signal_connect (notebook,
			  "tab-close-request",
			  G_CALLBACK (notebook_tab_close_request),
			  multi);
	g_signal_connect (notebook,
			  "button-press-event",
			  G_CALLBACK (notebook_button_press_event),
			  multi);
}

static void
disconnect_notebook_signals (GeditMultiNotebook *multi,
			     GtkWidget          *notebook)
{
	g_signal_handlers_disconnect_by_func (notebook, notebook_set_focus,
					      multi);
	g_signal_handlers_disconnect_by_func (notebook, notebook_switch_page,
					      multi);
	g_signal_handlers_disconnect_by_func (notebook, notebook_page_added,
					      multi);
	g_signal_handlers_disconnect_by_func (notebook, notebook_page_removed,
					      multi);
	g_signal_handlers_disconnect_by_func (notebook, notebook_tabs_reordered,
					      multi);
	g_signal_handlers_disconnect_by_func (notebook, notebook_tab_detached,
					      multi);
	g_signal_handlers_disconnect_by_func (notebook, notebook_tab_close_request,
					      multi);
	g_signal_handlers_disconnect_by_func (notebook, notebook_button_press_event,
					      multi);
}

static void
add_notebook (GeditMultiNotebook *multi,
	      GtkWidget          *notebook,
	      gboolean            main_container)
{
	if (main_container)
	{
		gtk_box_pack_start (GTK_BOX (multi), notebook, TRUE, TRUE, 0);
	}
	else
	{
		GtkWidget *paned;
		GtkWidget *parent;
		GtkAllocation allocation;
		GtkWidget *active_notebook = multi->priv->active_notebook;

		paned = gtk_hpaned_new ();
		gtk_widget_show (paned);

		/* First we remove the active container from its parent to make
		   this we add a ref to it*/
		g_object_ref (active_notebook);
		parent = gtk_widget_get_parent (active_notebook);
		gtk_widget_get_allocation (active_notebook, &allocation);

		gtk_container_remove (GTK_CONTAINER (parent), active_notebook);
		gtk_container_add (GTK_CONTAINER (parent), paned);

		gtk_paned_pack1 (GTK_PANED (paned), active_notebook, TRUE, FALSE);
		g_object_unref (active_notebook);

		gtk_paned_pack2 (GTK_PANED (paned), notebook, FALSE, FALSE);

		/* We need to set the new paned in the right place */
		gtk_paned_set_position (GTK_PANED (paned),
		                        allocation.width / 2);
	}

	multi->priv->notebooks = g_list_append (multi->priv->notebooks,
						notebook);

	gtk_widget_show (notebook);

	connect_notebook_signals (multi, notebook);

	g_signal_emit (G_OBJECT (multi), signals[NOTEBOOK_ADDED], 0, notebook);
}

static void
remove_notebook (GeditMultiNotebook *multi,
		 GtkWidget          *notebook)
{
	GtkWidget *parent;
	GtkWidget *grandpa;
	GList *children;
	GtkWidget *new_notebook;
	GList *current;

	if (multi->priv->notebooks->next == NULL)
	{
		g_warning ("You are trying to remove the main notebook");
		return;
	}

	current = g_list_find (multi->priv->notebooks,
			       notebook);

	if (current->next != NULL)
		new_notebook = GTK_WIDGET (current->next->data);
	else
		new_notebook = GTK_WIDGET (multi->priv->notebooks->data);

	parent = gtk_widget_get_parent (notebook);

	/* Now we destroy the widget, we get the children of parent and we destroy
	  parent too as the parent is an useless paned. Finally we add the child
	  into the grand parent */
	g_object_ref (notebook);
	multi->priv->removing_notebook = TRUE;

	gtk_widget_destroy (notebook);
	
	multi->priv->notebooks = g_list_remove (multi->priv->notebooks,
						notebook);

	multi->priv->removing_notebook = FALSE;
	
	/* Let's make the active notebook grab the focus */
	gtk_widget_grab_focus (new_notebook);

	children = gtk_container_get_children (GTK_CONTAINER (parent));
	if (children->next != NULL)
	{
		g_warning ("The parent is not a paned");
		return;
	}
	grandpa = gtk_widget_get_parent (parent);

	g_object_ref (children->data);
	gtk_container_remove (GTK_CONTAINER (parent),
			      GTK_WIDGET (children->data));
	gtk_widget_destroy (parent);
	gtk_container_add (GTK_CONTAINER (grandpa),
			   GTK_WIDGET (children->data));
	g_object_unref (children->data);

	disconnect_notebook_signals (multi, notebook);

	g_signal_emit (G_OBJECT (multi), signals[NOTEBOOK_REMOVED], 0, notebook);
	g_object_unref (notebook);
}

static void
gedit_multi_notebook_init (GeditMultiNotebook *multi)
{
	multi->priv = GEDIT_MULTI_NOTEBOOK_GET_PRIVATE (multi);

	multi->priv->removing_notebook = FALSE;

	multi->priv->active_notebook = gedit_notebook_new ();
	add_notebook (multi, multi->priv->active_notebook, TRUE);
}

GeditMultiNotebook *
gedit_multi_notebook_new ()
{
	return g_object_new (GEDIT_TYPE_MULTI_NOTEBOOK, NULL);
}

GeditNotebook *
gedit_multi_notebook_get_active_notebook (GeditMultiNotebook *multi)
{
	g_return_val_if_fail (GEDIT_IS_MULTI_NOTEBOOK (multi), NULL);

	return GEDIT_NOTEBOOK (multi->priv->active_notebook);
}

gint
gedit_multi_notebook_get_n_notebooks (GeditMultiNotebook *multi)
{
	g_return_val_if_fail (GEDIT_IS_MULTI_NOTEBOOK (multi), 0);

	return g_list_length (multi->priv->notebooks);
}

GeditNotebook *
gedit_multi_notebook_get_nth_notebook (GeditMultiNotebook *multi,
				       gint                notebook_num)
{
	g_return_val_if_fail (GEDIT_IS_MULTI_NOTEBOOK (multi), NULL);

	return g_list_nth_data (multi->priv->notebooks, notebook_num);
}

gint
gedit_multi_notebook_get_n_tabs (GeditMultiNotebook *multi)
{
	g_return_val_if_fail (GEDIT_IS_MULTI_NOTEBOOK (multi), 0);

	return multi->priv->total_tabs;
}

gint
gedit_multi_notebook_get_page_num (GeditMultiNotebook *multi,
				   GeditTab           *tab)
{
	GList *l;
	gint real_page_num = 0;

	for (l = multi->priv->notebooks; l != NULL; l = g_list_next (l))
	{
		gint page_num;

		page_num = gtk_notebook_page_num (GTK_NOTEBOOK (l->data),
						  GTK_WIDGET (tab));

		if (page_num != -1)
		{
			real_page_num += page_num;
			break;
		}

		real_page_num += gtk_notebook_get_n_pages (GTK_NOTEBOOK (l->data));
	}

	return real_page_num;
}

GeditTab *
gedit_multi_notebook_get_active_tab (GeditMultiNotebook *multi)
{
	g_return_val_if_fail (GEDIT_IS_MULTI_NOTEBOOK (multi), NULL);

	return (multi->priv->active_tab == NULL) ? 
				NULL : GEDIT_TAB (multi->priv->active_tab);
}

void
gedit_multi_notebook_set_active_tab (GeditMultiNotebook *multi,
				     GeditTab           *tab)
{
	GList *l;
	gint page_num;
	
	g_return_if_fail (GEDIT_IS_MULTI_NOTEBOOK (multi));
	g_return_if_fail (GEDIT_IS_TAB (tab));

	l = multi->priv->notebooks;

	do
	{
		page_num = gtk_notebook_page_num (GTK_NOTEBOOK (l->data),
						  GTK_WIDGET (tab));
		if (page_num != -1)
			break;

		l = g_list_next (l);
	} while (l != NULL && page_num == -1);

	g_return_if_fail (page_num != -1);

	gtk_notebook_set_current_page (GTK_NOTEBOOK (l->data),
				       page_num);

	if (GTK_WIDGET (l->data) != multi->priv->active_notebook)
	{
		gtk_widget_grab_focus (GTK_WIDGET (l->data));
	}
}

void
gedit_multi_notebook_set_current_page (GeditMultiNotebook *multi,
				       gint                page_num)
{
	GList *l;
	gint pages = 0;
	gint single_num = page_num;

	g_return_if_fail (GEDIT_IS_MULTI_NOTEBOOK (multi));
	
	for (l = multi->priv->notebooks; l != NULL; l = g_list_next (l))
	{
		gint p;
		
		p = gtk_notebook_get_n_pages (GTK_NOTEBOOK (l->data));
		pages += p;
		
		if ((pages - 1) >= page_num)
			break;

		single_num -= p;
	}

	if (GTK_WIDGET (l->data) != multi->priv->active_notebook)
	{
		gtk_widget_grab_focus (GTK_WIDGET (l->data));
	}

	gtk_notebook_set_current_page (GTK_NOTEBOOK (l->data), single_num);
}

GList *
gedit_multi_notebook_get_all_tabs (GeditMultiNotebook *multi)
{
	GList *nbs;
	GList *ret = NULL;

	g_return_val_if_fail (GEDIT_IS_MULTI_NOTEBOOK (multi), NULL);

	for (nbs = multi->priv->notebooks; nbs != NULL; nbs = g_list_next (nbs))
	{
		GList *l, *children;

		children = gtk_container_get_children (GTK_CONTAINER (nbs->data));

		for (l = children; l != NULL; l = g_list_next (l))
		{
			ret = g_list_prepend (ret, l->data);
		}
	}

	ret = g_list_reverse (ret);

	return ret;
}

void
gedit_multi_notebook_close_tabs (GeditMultiNotebook *multi,
				 const GList        *tabs)
{
	GList *l;

	g_return_if_fail (GEDIT_IS_MULTI_NOTEBOOK (multi));

	for (l = (GList *)tabs; l != NULL; l = g_list_next (l))
	{
		GList *nbs;

		for (nbs = multi->priv->notebooks; nbs != NULL; nbs = g_list_next (nbs))
		{
			gint n;

			n = gtk_notebook_page_num (GTK_NOTEBOOK (nbs->data),
						   GTK_WIDGET (l->data));

			if (n != -1)
				break;
		}

		gedit_notebook_remove_tab (GEDIT_NOTEBOOK (nbs->data),
					   GEDIT_TAB (l->data));
	}
}

/**
 * gedit_multi_notebook_close_all_tabs:
 * @multi: a #GeditMultiNotebook
 *
 * Closes all opened tabs.
 */
void
gedit_multi_notebook_close_all_tabs (GeditMultiNotebook *multi)
{
	GList *nbs, *l;

	g_return_if_fail (GEDIT_MULTI_NOTEBOOK (multi));

	/* We copy the list because the main one is going to have the items
	   removed */
	nbs = g_list_copy (multi->priv->notebooks);

	for (l = nbs; l != NULL; l = g_list_next (l))
	{
		gedit_notebook_remove_all_tabs (GEDIT_NOTEBOOK (l->data));
	}

	g_list_free (nbs);
}

void
gedit_multi_notebook_add_new_notebook (GeditMultiNotebook *multi)
{
	GtkWidget *notebook;
	GeditTab *tab;

	g_return_if_fail (GEDIT_IS_MULTI_NOTEBOOK (multi));

	notebook = gedit_notebook_new ();
	add_notebook (multi, notebook, FALSE);

	tab = GEDIT_TAB (_gedit_tab_new ());
	gtk_widget_show (GTK_WIDGET (tab));

	/* When gtk_notebook_insert_page is called the focus is set in
	   the notebook, we don't want this to happen until the page is added.
	   Also we don't want to call switch_page when we add the tab
	   but when we switch the notebook. */
	g_signal_handlers_block_by_func (notebook, notebook_set_focus, multi);
	g_signal_handlers_block_by_func (notebook, notebook_switch_page, multi);

	gedit_notebook_add_tab (GEDIT_NOTEBOOK (notebook),
				tab,
				-1,
				TRUE);

	g_signal_handlers_unblock_by_func (notebook, notebook_switch_page, multi);
	g_signal_handlers_unblock_by_func (notebook, notebook_set_focus, multi);

	notebook_set_focus (GTK_CONTAINER (notebook), NULL, multi);
}

void
gedit_multi_notebook_remove_active_notebook (GeditMultiNotebook *multi)
{
	g_return_if_fail (GEDIT_IS_MULTI_NOTEBOOK (multi));

	gedit_notebook_remove_all_tabs (GEDIT_NOTEBOOK (multi->priv->active_notebook));
}

void
gedit_multi_notebook_previous_notebook (GeditMultiNotebook *multi)
{
	GList *current;
	GtkWidget *notebook;

	g_return_if_fail (GEDIT_IS_MULTI_NOTEBOOK (multi));

	current = g_list_find (multi->priv->notebooks,
			       multi->priv->active_notebook);

	if (current->prev != NULL)
		notebook = GTK_WIDGET (current->prev->data);
	else
		notebook = GTK_WIDGET (g_list_last (multi->priv->notebooks)->data);

	gtk_widget_grab_focus (notebook);
}

void
gedit_multi_notebook_next_notebook (GeditMultiNotebook *multi)
{
	GList *current;
	GtkWidget *notebook;

	g_return_if_fail (GEDIT_IS_MULTI_NOTEBOOK (multi));

	current = g_list_find (multi->priv->notebooks,
			       multi->priv->active_notebook);

	if (current->next != NULL)
		notebook = GTK_WIDGET (current->next->data);
	else
		notebook = GTK_WIDGET (multi->priv->notebooks->data);

	gtk_widget_grab_focus (notebook);
}

void
gedit_multi_notebook_foreach_notebook (GeditMultiNotebook *mnb,
				       GtkCallback         callback,
				       gpointer            callback_data)
{
	GList *l;

	g_return_if_fail (GEDIT_IS_MULTI_NOTEBOOK (mnb));

	for (l = mnb->priv->notebooks; l != NULL; l = g_list_next (l))
	{
		callback (GTK_WIDGET (l->data), callback_data);
	}
}

void
gedit_multi_notebook_foreach_tab (GeditMultiNotebook *mnb,
				  GtkCallback         callback,
				  gpointer            callback_data)
{
	GList *nb;

	g_return_if_fail (GEDIT_IS_MULTI_NOTEBOOK (mnb));

	for (nb = mnb->priv->notebooks; nb != NULL; nb = g_list_next (nb))
	{
		GList *l, *children;

		children = gtk_container_get_children (GTK_CONTAINER (nb->data));

		for (l = children; l != NULL; l = g_list_next (l))
		{
			callback (GTK_WIDGET (l->data), callback_data);
		}

		g_list_free (children);
	}
}