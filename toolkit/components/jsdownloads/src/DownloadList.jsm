/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80 filetype=javascript: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Represents a collection of Download objects that can be viewed and managed by
 * the user interface, and persisted across sessions.
 */

"use strict";

this.EXPORTED_SYMBOLS = [
  "DownloadList",
];

////////////////////////////////////////////////////////////////////////////////
//// Globals

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;
const Cr = Components.results;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Promise",
                                  "resource://gre/modules/commonjs/sdk/core/promise.js");

////////////////////////////////////////////////////////////////////////////////
//// DownloadList

/**
 * Represents a collection of Download objects that can be viewed and managed by
 * the user interface, and persisted across sessions.
 */
function DownloadList() {
  this._downloads = [];
  this._views = new Set();
}

DownloadList.prototype = {
  /**
   * Array of Download objects currently in the list.
   */
  _downloads: null,

  /**
   * Retrieves a snapshot of the downloads that are currently in the list.  The
   * returned array does not change when downloads are added or removed, though
   * the Download objects it contains are still updated in real time.
   *
   * @return {Promise}
   * @resolves An array of Download objects.
   * @rejects JavaScript exception.
   */
  getAll: function DL_getAll() {
    return Promise.resolve(Array.slice(this._downloads, 0));
  },

  /**
   * Adds a new download to the end of the items list.
   *
   * @note When a download is added to the list, its "onchange" event is
   *       registered by the list, thus it cannot be used to monitor the
   *       download.  To receive change notifications for downloads that are
   *       added to the list, use the addView method to register for
   *       onDownloadChanged notifications.
   *
   * @param aDownload
   *        The Download object to add.
   */
  add: function DL_add(aDownload) {
    this._downloads.push(aDownload);
    aDownload.onchange = this._change.bind(this, aDownload);

    for (let view of this._views) {
      try {
        if (view.onDownloadAdded) {
          view.onDownloadAdded(aDownload);
        }
      } catch (ex) {
        Cu.reportError(ex);
      }
    }
  },

  /**
   * Removes a download from the list.  If the download was already removed,
   * this method has no effect.
   *
   * @param aDownload
   *        The Download object to remove.
   */
  remove: function DL_remove(aDownload) {
    let index = this._downloads.indexOf(aDownload);
    if (index != -1) {
      this._downloads.splice(index, 1);
      aDownload.onchange = null;

      for (let view of this._views) {
        try {
          if (view.onDownloadRemoved) {
            view.onDownloadRemoved(aDownload);
          }
        } catch (ex) {
          Cu.reportError(ex);
        }
      }
    }
  },

  /**
   * This function is called when "onchange" events of downloads occur.
   *
   * @param aDownload
   *        The Download object that changed.
   */
  _change: function DL_change(aDownload) {
    for (let view of this._views) {
      try {
        if (view.onDownloadChanged) {
          view.onDownloadChanged(aDownload);
        }
      } catch (ex) {
        Cu.reportError(ex);
      }
    }
  },

  /**
   * Set of currently registered views.
   */
  _views: null,

  /**
   * Adds a view that will be notified of changes to downloads.  The newly added
   * view will receive onDownloadAdded notifications for all the downloads that
   * are already in the list.
   *
   * @param aView
   *        The view object to add.  The following methods may be defined:
   *        {
   *          onDownloadAdded: function (aDownload) {
   *            // Called after aDownload is added to the end of the list.
   *          },
   *          onDownloadChanged: function (aDownload) {
   *            // Called after the properties of aDownload change.
   *          },
   *          onDownloadRemoved: function (aDownload) {
   *            // Called after aDownload is removed from the list.
   *          },
   *        }
   */
  addView: function DL_addView(aView)
  {
    this._views.add(aView);

    if (aView.onDownloadAdded) {
      for (let download of this._downloads) {
        try {
          aView.onDownloadAdded(download);
        } catch (ex) {
          Cu.reportError(ex);
        }
      }
    }
  },

  /**
   * Removes a view that was previously added using addView.  The removed view
   * will not receive any more notifications after this method returns.
   *
   * @param aView
   *        The view object to remove.
   */
  removeView: function DL_removeView(aView)
  {
    this._views.delete(aView);
  },
};
