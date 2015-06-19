$(document).ready(function() {
    $("#titleBar").click(onclickSlideUp);
});

function onclickSlideUp() {
    $content = $("#Container");
    $content.slideUp(500, function() {
        $("#titleBar").unbind("click");
        $("#titleBar").click(onclickSlideDown);
    });
}

function onclickSlideDown() {
    $content = $("#Container");
    $content.slideDown(500, function () {
        $("#titleBar").unbind("click");
        $("#titleBar").click(onclickSlideUp);
    });
}