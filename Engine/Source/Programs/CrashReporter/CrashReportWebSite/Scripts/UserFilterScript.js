$(document).ready(function () {
    $("#UserName").autocomplete({
        source: function (request, response) {
            $.ajax({
                url: "/Users/AutocompleteUser",
                type: "POST",
                dataType: "json",
                data: { userName: request.term },
                success: function (data) {
                    response($.map(data, function (item) {
                        return { label: item.UserName, value: item.UserName, group: item.Group };
                    }));
                }
            });
        },
        select: function(event, ui) {
            var label = ui.item.group;
            $("#UserGroup").val(label);
        },
        messages: {
            noResults: "", results: ""
        }
    });
});
