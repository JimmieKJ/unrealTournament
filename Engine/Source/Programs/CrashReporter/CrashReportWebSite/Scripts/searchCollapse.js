$(document).ready(function ()
{
	$("#TitleBar").click(onclickSlideUp);
});

function onclickSlideUp()
{
	$content = $("#Container");
	$content.slideUp(250, function ()
	{
		$("#TitleBar").unbind("click");
		$("#TitleBar").click(onclickSlideDown);
	});
}

function onclickSlideDown()
{
	$content = $("#Container");
	$content.slideDown(250, function ()
	{
		$("#TitleBar").unbind("click");
		$("#TitleBar").click(onclickSlideUp);
	});
}