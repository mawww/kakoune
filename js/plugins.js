"use strict";

(function () {
    $(document).ready(function () {
        const SEARCH_QUERY = "topic:plugin topic:kakoune";
        const MAX_ITEMS_PAGE = 50;
        const URL_ENDPOINT = "https://api.github.com/search/repositories";
        const URL_DATA = { q: SEARCH_QUERY, per_page: MAX_ITEMS_PAGE };

        var $window = $(window),
            $document = $(document);

        var $pluginsList = $("#pluginsList"),
            $pluginsListContainer = $("#pluginsListContainer"),
            $pluginTmpl = $("#pluginTmpl");

        function escapeHtml(str) {
            var e = document.createElement("span");

            e.appendChild(document.createTextNode(str));

            return e.innerHTML;
        }

        function getPlugins(url, url_data, on_complete) {
            $.getJSON(url, url_data)
                .done(function (data) {
                    if ("items" in data) {
                        var pluginsContext = [];

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
                        // NOTE: the `loadTemplate` function doesn't call the callback when no items are passed to it
                        if (!data.items.length) {
                            return on_complete(0);
                        }

                        $pluginsList.loadTemplate($pluginTmpl, pluginsContext, {
                            append: true,
                            isFile: false,
                            complete: function () {
                                return on_complete(pluginsContext.length);
                            },
                        });
                    } else {
                        alert("Corrupted data received by the endpoint, check the console log");
                        console.log(data);
                    }
                })
                .fail(function (_, __, error) {
                    alert("Unable to fetch plugin page: " + error);
                });
        }

        function getPage(url, url_data, page, on_complete) {
            var data = url_data;

            data.page = page;
            getPlugins(url, data, on_complete);
        }

        function on_complete_cb(nb_items_loaded, shared_data, url, url_data) {
            if (nb_items_loaded) {
                shared_data.page++;
                fillPage(url, url_data, shared_data);
            } else {
                shared_data.max_page_reached = true;
            }
        }

        function fillPage(url, url_data, shared_data) {
            if ($pluginsListContainer.height() <= $window.height()) {
                getPage(url, url_data, shared_data.page, function (nb_items_loaded) {
                    on_complete_cb(nb_items_loaded, shared_data, url, url_data);
                });
            }
        }

        var shared_data = {
            page: 1,
            max_page_reached: false,
        };

        fillPage(URL_ENDPOINT, URL_DATA, shared_data);

        $window.scroll(function () {
            if ($document.height() - $window.height() === $window.scrollTop()
                && !shared_data.max_page_reached) {
                getPage(URL_ENDPOINT, URL_DATA, shared_data.page, function (nb_items_loaded) {
                    on_complete_cb(nb_items_loaded, shared_data, URL_ENDPOINT, URL_DATA);
                });
            }
        });
    });
})();
