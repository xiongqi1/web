// same height
function equalHeight(group) {
	var tallest = 0;
	group.each(function() {
		var thisHeight = $(this).height();
		if(thisHeight > tallest) {
			tallest = thisHeight;
		}
	});
	group.height(tallest);
}
jQuery(function() {
	equalHeight($("#system-information .each-box"));
	equalHeight($("#connection-status .each-box"));
});

// collapse
jQuery(function() {
	// box header
	$(".box-header").click(function() {
	    $(this).next("div").toggleClass("hide");
		$(this).toggleClass("close");
	});
	
	// sidemenu
	$(".sidemenu li:has(div)").toggleClass("child");
  $(".sidemenu li.child a").click(function () {
    if ($(this).parent().has("div")) {
      $(this).parent().toggleClass('open').children("div").toggleClass("hide");
      }
  });

});

jQuery(function() {
  // Add/Remove 'focus' classes to input[type=password] fields
  $('input[type=password]').focus(function() { 
	$(this).addClass('focus');
	$(this).parent().addClass('focus');
  });
  $('input[type=password]').focusout(function() { 
	$(this).removeClass('focus');
	$(this).parent().removeClass('focus');
  });

  // UJS
  $('div.radio-switch').each(function(index, e) {
    if(t = $(e).attr('data-toggle-element')) {
      target = t; // Cannot use the if-scoped variable in callback functions!
      $($(e).children('input')[0]).click(function() {
        if(dis = $(this).parent().attr('data-toggle-disabled')) {
          $('#' + dis).removeClass('disabled');
        }
        $('#' + target).show();
      });
      $($(e).children('input')[1]).click(function() {
        if(dis = $(this).parent().attr('data-toggle-disabled')) {
          $('#' + dis).addClass('disabled');
        }
        $('#' + target).hide();
      });
    }
  });
  $('button.toggle-area').click(function() {
	if(t = $(this).attr('data-toggle-selector')) {
	  $(t).toggle();
	  if(alt_title = $(this).attr('data-title-alternative')) {
		$(this).attr('data-title-alternative', $(this).html());
		if(alt_title.indexOf("_(")!=-1)
		  $(this).html(eval(alt_title));
		else
		  $(this).html(alt_title);
	  }
	}
	return false;
  });
});