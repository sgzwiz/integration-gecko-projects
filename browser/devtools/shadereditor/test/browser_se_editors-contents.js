/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the editors contain the correct text when a program
 * becomes available.
 */

function ifWebGLSupported() {
  let [target, debuggee, panel] = yield initShaderEditor(SIMPLE_CANVAS_URL);
  let { gFront, ShadersEditorsView } = panel.panelWin;

  reload(target);
  yield once(gFront, "program-linked");

  let vsEditor = yield ShadersEditorsView._getEditor("vs");
  let fsEditor = yield ShadersEditorsView._getEditor("fs");

  is(vsEditor.getText().indexOf("gl_Position"), 170,
    "The vertex shader editor contains the correct text.");
  is(fsEditor.getText().indexOf("gl_FragColor"), 97,
    "The fragment shader editor contains the correct text.");

  yield teardown(panel);
  finish();
}
