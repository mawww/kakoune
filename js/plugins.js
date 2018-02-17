"use strict";

(function () {
    $(document).ready(function () {
        function escapeHtml(str) {
            var e = document.createElement("span");

            e.appendChild(document.createTextNode(str));

            return e.innerHTML;
        }

        const URL_ENDPOINT = "https://api.github.com/search/repositories";
        const DATA_ENDPOINT = { q: "topic:kakoune topic:plugin" };

        $.getJSON(URL_ENDPOINT, DATA_ENDPOINT)
            .done(function (data) {
                if ("items" in data) {
                    var pluginTmpl = $("#pluginTmpl"),
                        pluginsList = $("#pluginsList"),
                        pluginsContext = [];

                    $.each(data.items, function (_, e) {
                        pluginsContext.push({
                            owner: {
                                username: escapeHtml(e.owner.login),
                                profile_url: escapeHtml(e.owner.html_url),
                                avatar_url: escapeHtml(e.owner.avatar_url),
                            },
                            plugin: {
                                name: escapeHtml(e.name),
                                stars: escapeHtml(e.stargazers_count),
                                repository_url: escapeHtml(e.html_url),
                                description: escapeHtml(e.description),
                            },
                        });
                    });

                    pluginsList.loadTemplate(pluginTmpl, pluginsContext, {
                        append: true,
                        isFile: false,
                    });
                } else {
                    alert("Corrupted data received by the endpoint, check the console log");
                    console.log(data);
                }
            })
            .fail(function (_, __, error) {
                alert("Unable to fetch plugin list: " + error);
            });
    });
})();
