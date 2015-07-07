/*
 * Copyright (C) 2013 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "WebKitWebViewGroup.h"

#include "APIArray.h"
#include "APIString.h"
#include "WebKitPrivate.h"
#include "WebKitSettingsPrivate.h"
#include "WebKitWebViewGroupPrivate.h"
#include <glib/gi18n-lib.h>
#include <wtf/gobject/GRefPtr.h>
#include <wtf/text/CString.h>

using namespace WebKit;

/**
 * SECTION: WebKitWebViewGroup
 * @Short_description: Group of web views
 * @Title: WebKitWebViewGroup
 * @See_also: #WebKitWebView, #WebKitSettings
 *
 * A WebKitWebViewGroup represents a group of #WebKitWebView<!-- -->s that
 * share things like settings. There's a default WebKitWebViewGroup where
 * all #WebKitWebView<!-- -->s of the same #WebKitWebContext are added by default.
 * To create a #WebKitWebView in a different WebKitWebViewGroup you can use
 * webkit_web_view_new_with_group().
 *
 * WebKitWebViewGroups are identified by a unique name given when the group is
 * created with webkit_web_view_group_new().
 * WebKitWebViewGroups have a #WebKitSettings to control the settings of all
 * #WebKitWebView<!-- -->s of the group. You can get the settings with
 * webkit_web_view_group_get_settings() to handle the settings, or you can set
 * your own #WebKitSettings with webkit_web_view_group_set_settings(). When
 * the #WebKitSettings of a WebKitWebViewGroup changes, the signal notify::settings
 * is emitted on the group.
 */

enum {
    PROP_0,

    PROP_SETTINGS
};

struct _WebKitWebViewGroupPrivate {
    RefPtr<WebPageGroup> pageGroup;
    CString name;
    GRefPtr<WebKitSettings> settings;
};

WEBKIT_DEFINE_TYPE(WebKitWebViewGroup, webkit_web_view_group, G_TYPE_OBJECT)

static inline WebCore::UserContentInjectedFrames toWebCoreUserContentInjectedFrames(WebKitInjectedContentFrames kitFrames)
{
    switch (kitFrames) {
    case WEBKIT_INJECTED_CONTENT_FRAMES_ALL:
        return WebCore::InjectInAllFrames;
    case WEBKIT_INJECTED_CONTENT_FRAMES_TOP_ONLY:
        return WebCore::InjectInTopFrameOnly;
    default:
        ASSERT_NOT_REACHED();
        return WebCore::InjectInAllFrames;
    }
}

static void webkitWebViewGroupSetProperty(GObject* object, guint propId, const GValue* value, GParamSpec* paramSpec)
{
    WebKitWebViewGroup* group = WEBKIT_WEB_VIEW_GROUP(object);

    switch (propId) {
    case PROP_SETTINGS:
        webkit_web_view_group_set_settings(group, WEBKIT_SETTINGS(g_value_get_object(value)));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propId, paramSpec);
    }
}

static void webkitWebViewGroupGetProperty(GObject* object, guint propId, GValue* value, GParamSpec* paramSpec)
{
    WebKitWebViewGroup* group = WEBKIT_WEB_VIEW_GROUP(object);

    switch (propId) {
    case PROP_SETTINGS:
        g_value_set_object(value, webkit_web_view_group_get_settings(group));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propId, paramSpec);
    }
}

static void webkitWebViewGroupConstructed(GObject* object)
{
    G_OBJECT_CLASS(webkit_web_view_group_parent_class)->constructed(object);

    WebKitWebViewGroupPrivate* priv = WEBKIT_WEB_VIEW_GROUP(object)->priv;
    priv->settings = adoptGRef(webkit_settings_new());
}

static void webkit_web_view_group_class_init(WebKitWebViewGroupClass* hitTestResultClass)
{
    GObjectClass* objectClass = G_OBJECT_CLASS(hitTestResultClass);
    objectClass->set_property = webkitWebViewGroupSetProperty;
    objectClass->get_property = webkitWebViewGroupGetProperty;
    objectClass->constructed = webkitWebViewGroupConstructed;

    /**
     * WebKitWebViewGroup:settings:
     *
     * The #WebKitSettings of the web view group.
     */
    g_object_class_install_property(
        objectClass,
        PROP_SETTINGS,
        g_param_spec_object(
            "settings",
            _("Settings"),
            _("The settings of the web view group"),
            WEBKIT_TYPE_SETTINGS,
            WEBKIT_PARAM_READWRITE));
}

static void webkitWebViewGroupAttachSettingsToPageGroup(WebKitWebViewGroup* group)
{
    group->priv->pageGroup->setPreferences(webkitSettingsGetPreferences(group->priv->settings.get()));
}

WebKitWebViewGroup* webkitWebViewGroupCreate(WebPageGroup* pageGroup)
{
    WebKitWebViewGroup* group = WEBKIT_WEB_VIEW_GROUP(g_object_new(WEBKIT_TYPE_WEB_VIEW_GROUP, NULL));
    group->priv->pageGroup = pageGroup;
    webkitWebViewGroupAttachSettingsToPageGroup(group);
    return group;
}

WebPageGroup* webkitWebViewGroupGetPageGroup(WebKitWebViewGroup* group)
{
    return group->priv->pageGroup.get();
}

/**
 * webkit_web_view_group_new:
 * @name: (allow-none): the name of the group
 *
 * Creates a new #WebKitWebViewGroup with the given @name.
 * If @name is %NULL a unique identifier name will be created
 * automatically.
 * The newly created #WebKitWebViewGroup doesn't contain any
 * #WebKitWebView, web views are added to the new group when created
 * with webkit_web_view_new_with_group() passing the group.
 *
 * Returns: (transfer full): a new #WebKitWebViewGroup
 */
WebKitWebViewGroup* webkit_web_view_group_new(const char* name)
{
    WebKitWebViewGroup* group = WEBKIT_WEB_VIEW_GROUP(g_object_new(WEBKIT_TYPE_WEB_VIEW_GROUP, NULL));
    group->priv->pageGroup = WebPageGroup::create(name ? String::fromUTF8(name) : String());
    webkitWebViewGroupAttachSettingsToPageGroup(group);
    return group;
}

/**
 * webkit_web_view_group_get_name:
 * @group: a #WebKitWebViewGroup
 *
 * Gets the name that uniquely identifies the #WebKitWebViewGroup.
 *
 * Returns: the name of @group
 */
const char* webkit_web_view_group_get_name(WebKitWebViewGroup* group)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW_GROUP(group), 0);

    WebKitWebViewGroupPrivate* priv = group->priv;
    if (priv->name.isNull())
        priv->name = priv->pageGroup->identifier().utf8();

    return priv->name.data();
}

/**
 * webkit_web_view_group_get_settings:
 * @group: a #WebKitWebViewGroup
 *
 * Gets the #WebKitSettings of the #WebKitWebViewGroup.
 *
 * Returns: (transfer none): the settings of @group
 */
WebKitSettings* webkit_web_view_group_get_settings(WebKitWebViewGroup* group)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW_GROUP(group), 0);

    return group->priv->settings.get();
}

/**
 * webkit_web_view_group_set_settings:
 * @group: a #WebKitWebViewGroup
 * @settings: a #WebKitSettings
 *
 * Sets a new #WebKitSettings for the #WebKitWebViewGroup. The settings will
 * affect to all the #WebKitWebView<!-- -->s of the group.
 * #WebKitWebViewGroup<!-- -->s always have a #WebKitSettings so if you just want to
 * modify a setting you can use webkit_web_view_group_get_settings() and modify the
 * returned #WebKitSettings instead.
 * Setting the same #WebKitSettings multiple times doesn't have any effect.
 * You can monitor the settings of a #WebKitWebViewGroup by connecting to the
 * notify::settings signal of @group.
 */
void webkit_web_view_group_set_settings(WebKitWebViewGroup* group, WebKitSettings* settings)
{
    g_return_if_fail(WEBKIT_IS_WEB_VIEW_GROUP(group));
    g_return_if_fail(WEBKIT_IS_SETTINGS(settings));

    if (group->priv->settings == settings)
        return;

    group->priv->settings = settings;
    webkitWebViewGroupAttachSettingsToPageGroup(group);
    g_object_notify(G_OBJECT(group), "settings");
}
