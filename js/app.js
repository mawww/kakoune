"use strict";

(function () {
    var cs_demos = $("#carousel-demos");

    if (cs_demos) {
        cs_demos.on("slide.bs.carousel", function (e) {
            var video = $(e.relatedTarget).find("video");
            if (video) {
                video[0].currentTime = 0;
                video[0].pause();
            }
        });
        cs_demos.on("slid.bs.carousel", function (e) {
            var video = $(e.relatedTarget).find("video");
            if (video) {
                video[0].play();
            }
        });
    }
})();
