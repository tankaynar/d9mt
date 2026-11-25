// osascript -l JavaScript activate.js <pid>... — bring a Wine game process to the front (like clicking its window).
// Prints one status line. Uses NSRunningApplication, so no Accessibility permission is needed.
ObjC.import('AppKit');
function run(argv) {
  var out = [];
  argv.forEach(function (s) {
    var pid = parseInt(s, 10);
    var app = $.NSRunningApplication.runningApplicationWithProcessIdentifier(pid);
    if (!app || app.isNil()) { out.push(pid + ':no-app'); return; }
    out.push(pid + ':' + ObjC.unwrap(app.localizedName) + ' policy=' + app.activationPolicy + ' active=' + app.active);
    app.activateWithOptions(3);
  });
  var f = $.NSWorkspace.sharedWorkspace.frontmostApplication;
  out.push('front=' + (f ? ObjC.unwrap(f.localizedName) + '/' + f.processIdentifier : 'none'));
  return out.join(' | ');
}
