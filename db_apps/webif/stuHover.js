/* ================================================================ 
This copyright notice must be kept untouched in the stylesheet at 
all times.

The original version of this script and the associated (x)html
is available at http://www.stunicholls.com/menu/pro_drop_1.html
Copyright (c) 2005-2007 Stu Nicholls. All rights reserved.
This script and the associated (x)html may be modified in any 
way to fit your requirements.
=================================================================== */
var stuHover = function() {
	var cssRule;
	var newSelector;
	var getElm;
	for (var i = 0; i < document.styleSheets.length; i++)
	{
		if( document.styleSheets[i].rules == null ) break;
		for (var x = 0; x < document.styleSheets[i].rules.length ; x++)
			{
			cssRule = document.styleSheets[i].rules[x];
			if (cssRule.selectorText.indexOf("LI:hover") != -1)
			{
				 newSelector = cssRule.selectorText.replace(/LI:hover/gi, "LI.iehover");
				document.styleSheets[i].addRule(newSelector , cssRule.style.cssText);
			}
		}
	}
	var nav=document.getElementById("nav");
	if( nav && (getElm = nav.getElementsByTagName("li"))!=null )
	{	
		for (var i=0; i<getElm.length; i++) {
			getElm[i].onmouseover=function() {
				this.className+=" iehover";
			}
			getElm[i].onmouseout=function() {
				this.className=this.className.replace(new RegExp(" iehover\\b"), "");
			}
		}
	}	
	var nav2=document.getElementById("nav2");
	if( nav2 && (getElm = nav2.getElementsByTagName("li"))!=null )
	{
		for (var i=0; i<getElm.length; i++) {
			getElm[i].onmouseover=function() {
				this.className+=" iehover";
			}
			getElm[i].onmouseout=function() {
				this.className=this.className.replace(new RegExp(" iehover\\b"), "");
			}
		}	
	}
}
if (window.attachEvent) window.attachEvent("onload", stuHover);

