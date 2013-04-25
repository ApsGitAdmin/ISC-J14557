(function ($) {
	$('iframe').load(function(){
		// Copies the content of the title field into the Drupal title
    	$('iframe').contents().find('input#presentation_name').bind('change',function(e) {
            title_name = $(this).val();
            $('input#edit-title').val(title_name);
         });

    	// Disables the input button until a file has been added to the form.
    	$('iframe').contents().find('input#presentation_file').bind('change',function(e) {
            if ($(this).val()) {
                $('iframe').contents().find('input#presentation_submit').removeAttr('disabled');
            } 
        });
	});
})(jQuery);