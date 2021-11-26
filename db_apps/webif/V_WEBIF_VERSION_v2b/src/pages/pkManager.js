var Msgs = [];
function pkDetailWindow(idx) {
  var detailWindow = window.open("", "", "resizable=yes,scrollbars=yes,toolbar=no,width=800,height=800,top=100,left=200");
  detailWindow.document.write(Msgs[idx]);
  detailWindow.document.close();
}

function pkUninstall(pkgName) {
  var msg = _("Msg112")+" '" + pkgName + "'?";//Do you really want to uninstall this package
  pageData.pageObjects[0].obj.forEach(function(pkg) {
    if(pkg.Package === pkgName) {
      pkg.filelist.some(function(file) {
        if( file.indexOf("/etc/appweb/appweb.conf") >= 0) {
          msg = _("Msg111") + msg; //Stop! This package contains the web server configuration file,uninstall this package will disable the web server!
          return true;
        }
        return false;
      });
    }
  });
  blockUI_confirm(msg, function() {
    var objs = [];
    var obj: any = {};
    obj.name = "PkgMgr";
    obj.uninstall = pkgName;
    objs[0] = obj;
    sendTheseObjects(objs)
  });
}

class InstallTimeVariable extends StaticTextVariable {
  setVal(obj: any, val: string) {
    var installedTime = new Date(Number(val)*1000);
    $("#"+this.getHtmlId()).html(installedTime.toLocaleString());
  }
}

class DetailsLinkVariable extends ShownVariable {
  setVal(obj: any, val: string) {
    var title = "";
    if ( obj && obj.control && obj.control.Description)
      title = "Description: " + obj.control.Description + "\n";
    if (obj && obj.filelist)
      title += obj.filelist.join("\n");

    var msg = "";
    if ( obj && obj.control && obj.control.Description)
      msg = "Description:<br>" + obj.control.Description + "<br>";
    if (obj && obj.filelist)
      msg += "<br>" + _('package details') + "<br>" + obj.filelist.join("<br>");

    var MsgIndex = Msgs.length;
    Msgs[MsgIndex] = msg;

    var el:any = document.getElementById("link_" + this.getHtmlId());
    if (el) {
      el.href = "javascript:pkDetailWindow(" + MsgIndex + ")";
      el.title = title;
      el.innerHTML = _(this.labelText);
    }
  }
  genHtml() { return this.genLink(); }
}

class UninstallLinkVariable extends ShownVariable {
  setVal(obj: any, val: string) {
    var el: any = document.getElementById("link_" + this.getHtmlId());
    if (el) {
      el.href = "javascript:pkUninstall('" + obj.Package + "')";
      el.title = _(this.labelText)+" " + obj.Package;
      el.innerHTML = _(this.labelText);
    }
  }
  genHtml() { return this.genLink(); }
}

var pkgMgr = PageTableObj("PkgMgr", "pkg manager",
{
  customLua: true,
  members: [
    staticTextVariable("Package", "packageName"),
    staticTextVariable("Version", "version"),
    staticTextVariable("Architecture", "architecture"),
    new InstallTimeVariable("Installed-Time", "installedTime"),
    new DetailsLinkVariable("filelist", "package details"),
    new UninstallLinkVariable("uninstall", "uninstall"),
  ]
});

var pageData : PageDef = {
#if defined V_CUSTOM_FEATURE_PACK_bellca || defined V_WEBIF_SPEC_lark || defined V_SYSTEM_CONFIG_UI_none
  onDevice : false,
#endif
  title: "NetComm Wireless Router",
  menuPos: ["System", "PKG_MANAGER"],
  rootOnly: true,
  pageObjects: [pkgMgr]
}
