# -*- Mode: javascript; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

let AddonParent = {
  init: function() {
    messageManager.addMessageListener("Addon:Observe", this);
    messageManager.addMessageListener("Addon:ShouldLoad", this);

    var styleSheets = Cc["@mozilla.org/content/style-sheet-service;1"].getService(Ci.nsIStyleSheetService);
    var list = styleSheets.enumerateStyleSheets(1);
    while (list.hasMoreElements()) {
      var item = list.getNext();
      messageManager.broadcastAsyncMessage("StyleSheet:Load", {"href":item.href});
    }

    Services.obs.addObserver(function(sheet, topic, data) {
      messageManager.broadcastAsyncMessage("StyleSheet:Load", {"href":sheet.href});
    }, "user-sheet-added", false);
  },

  receiveMessage: function (message) {
    switch (message.name) {
    case "Addon:Observe":
      break;

    case "Addon:ShouldLoad":
      return this.shouldLoad(message.target, message.json, message.objects);
      break;
    }
  },

  shouldLoad: function(target, json, remote) {
    var contentLocation = remote.contentLocation;
    var requestOrigin = remote.requestOrigin;
    var node = remote.node;
    var contentType = json.contentType;
    var mimeTypeGuess = json.mimeTypeGuess;

    var catMan = Cc["@mozilla.org/categorymanager;1"].getService(Ci.nsICategoryManager);
    var list = catMan.enumerateCategory("content-policy");
    while (list.hasMoreElements()) {
      var item = list.getNext();
      var service = item.QueryInterface(Components.interfaces.nsISupportsCString).toString();
      if (!(service in Cc))
        continue;
      var policy = Cc[service].getService(Ci.nsIContentPolicy);
      if (!Cu.isWrappedJS(policy))
        continue;
      var r = policy.shouldLoad(contentType,
                                contentLocation,
                                requestOrigin,
                                node,
                                mimeTypeGuess,
                                null);
      if (r != Ci.nsIContentPolicy.ACCEPT && r != 0)
        return r;
    }

    return Ci.nsIContentPolicy.ACCEPT;
  }
};

let BrowserParent = {
  init: function() {
    AddonParent.init();
    messageManager.addMessageListener("Content:Click", this);
  },

  receiveMessage: function (message) {
    switch (message.name) {
    case "Content:Click":
      this.remoteContentAreaClick(message.json, message.target)
      break;
    }
  },

  remoteContentAreaClick: function (json, browser) {
    // This is heavily based on contentAreaClick from browser.js

    if (!json.href) {
      // Might be middle mouse navigation.
      if (gPrefService.getBoolPref("middlemouse.contentLoadURL") &&
          !gPrefService.getBoolPref("general.autoScroll")) {
        middleMousePaste(json);
      }
      return;
    }

    if (json.bookmark) {
      // This is the Opera convention for a special link that, when clicked,
      // allows to add a sidebar panel.  The link's title attribute contains
      // the title that should be used for the sidebar panel.
      PlacesUIUtils.showBookmarkDialog({ action: "add"
                                       , type: "bookmark"
                                       , uri: makeURI(json.href)
                                       , title: json.title
                                       , loadBookmarkInSidebar: true
                                       , hiddenRows: [ "description"
                                                     , "location"
                                                     , "keyword" ]
                                       }, window);
      return;
    }

    // Todo: maybe code for sidebars?

    // This part is based on handleLinkClick.
    var where = whereToOpenLink(json);
    if (where == "current")
      return false;

    // Todo: code for where == save

    openLinkIn(json.href, where, { referrerURI: browser.documentURI,
                                   charset: browser.characterSet });

    // Mark the page as a user followed link.  This is done so that history can
    // distinguish automatic embed visits from user activated ones.  For example
    // pages loaded in frames are embed visits and lost with the session, while
    // visits across frames should be preserved.
    try {
      if (!PrivateBrowsingUtils.isWindowPrivate(window))
        PlacesUIUtils.markPageAsFollowedLink(href);
    } catch (ex) { /* Skip invalid URIs. */ }
  }

};

function SSLStatusProvider(browser)
{
  this._browser = browser;
  this._state = 0;
  this._SSLStatus = null;
}

SSLStatusProvider.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsISSLStatusProvider, Ci.nsISecureBrowserUI]),

  get state() { return this._state; },
  get SSLStatus() { return this._SSLStatus; },

  _update: function (state, status) {
      let deserialized = null;
      if (status) {
        let helper = Cc["@mozilla.org/network/serialization-helper;1"]
                      .getService(Components.interfaces.nsISerializationHelper);

        deserialized = helper.deserializeObject(status)
        deserialized.QueryInterface(Ci.nsISSLStatus);
      }

      // We must check the Extended Validation (EV) state here, on the chrome
      // process, because NSS is needed for that determination.
      if (deserialized && deserialized.isExtendedValidation)
        state |= Ci.nsIWebProgressListener.STATE_IDENTITY_EV_TOPLEVEL;

      this._state = state;
      this._SSLStatus = deserialized;
  }
}
