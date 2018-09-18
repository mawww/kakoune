"use strict";

(function () {
    var translation_lang = "en";
    var translations = {
        "en": {},
        "fr": {
            "head:indexPageTitle": "Kakoune - Site officiel",
            "head:galleryPageTitle": "Kakoune - Galerie",
            "head:pluginsPageTitle": "Kakoune - Extensions",

            "navbar:getStarted": "Commencer !",
            "navbar:documentation": "Documentation",
            "navbar:issueTracker": "Gestionnaire de tickets",
            "navbar:wiki": "Wiki",
            "navbar:designNotes": "Notes de conception",
            "navbar:gallery": "Galerie",
            "navbar:plugins": "Extensions",
            "navbar:translation": "Traduction",
            "navbar:translation:english": "Anglais",
            "navbar:translation:french": "Français",

            "banner:title": "Kakoune : l'éditeur de code",
            "banner:pitch": "Editeur modal · Plus rapide via moins de touches · Sélections multiples · Conception orthogonale",

            "menu:features": "Fonctionnalités",
            "menu:screenshots": "Captures d'écran",
            "menu:gallery": "Galerie",

            "feature:multipleSelections": "Sélections multiples",
            "feature:multipleSelectionsText": $("[tid-fr='feature:multipleSelectionsText']").html(),
            "feature:textEditingTools": "Outils d'édition de texte",
            "feature:textEditingToolsText": $("[tid-fr='feature:textEditingToolsText']").html(),
            "feature:advancedTextManipulation": "Primitives de manipulation de texte avancées",
            "feature:advancedTextManipulationText": $("[tid-fr='feature:advancedTextManipulationText']").html(),
            "feature:customization": "Personnalisation",
            "feature:customizationText": $("[tid-fr='feature:customizationText']").html(),
            "feature:clientServerArchitecture": "Architecture client/serveur",
            "feature:clientServerArchitectureText": $("[tid-fr='feature:clientServerArchitectureText']").html(),
            "feature:activeDevelopmentSupport": "Développement actif & support",
            "feature:activeDevelopmentSupportText": $("[tid-fr='feature:activeDevelopmentSupportText']").html(),
            "feature:learnMore": "Découvrir la philosophie Kakoune (en anglais)",

            "error:noVideoTag": "Votre navigateur ne gère pas l'élément <code>video</code>.",

            "demo:labelNext": "Suivant",
            "demo:labelPrevious": "Précédent",

            "demo:0": "Aligner automatiquement des symboles, en alignant les curseurs des sélections",
            "demo:1": "Sélection de texte via prédicat, sauvegardé dans le cache interne pour de futures manipulations",
            "demo:2": "Remplacement de texte visuel et interactif dans une sélection",
            "demo:3": "Echange simplifié de paramètres grâce à la commande de rotation de sélections",
            "demo:4": "Architecture client/serveur, utilisation du gestionnaire de fenêtres X11, lancement de make/grep en fond",
            "demo:5": "Limitation du nombre de caractères des lignes qui ne sont pas des questions dans une FAQ",

            "screenshot:0": $("[tid-fr='screenshot:0']").html(),
            "screenshot:1": $("[tid-fr='screenshot:1']").html(),

            "gallery:0": $("[tid-fr='gallery:0']").html(),
            "gallery:1": $("[tid-fr='gallery:1']").html(),
            "gallery:2": $("[tid-fr='gallery:2']").html(),
            "gallery:3": $("[tid-fr='gallery:3']").html(),
            "gallery:4": $("[tid-fr='gallery:4']").html(),
            "gallery:5": $("[tid-fr='gallery:5']").html(),
            "gallery:6": $("[tid-fr='gallery:6']").html(),
            "gallery:7": $("[tid-fr='gallery:7']").html(),
            "gallery:8": $("[tid-fr='gallery:8']").html(),
            "gallery:9": $("[tid-fr='gallery:9']").html(),
            "gallery:10": $("[tid-fr='gallery:10']").html(),
            "gallery:11": $("[tid-fr='gallery:11']").html(),
            "gallery:12": $("[tid-fr='gallery:12']").html(),
            "gallery:13": $("[tid-fr='gallery:13']").html(),

            "plugins:malwareWarning": $("[tid-fr='plugins:malwareWarning']").html(),
            "plugins:infoContribute": $("[tid-fr='plugins:infoContribute']").html(),
        },
    }

    function translate_to_lang(lang) {
        translation_lang = lang;
        window.localStorage.setItem("translation-lang", lang);

        $("[tid]").each(function (i, el) {
            var $el = $(el),
                translation_id = $el.attr("tid");

            if (lang in translations) {
                if (translation_id in translations[lang]) {
                    var html = translations[lang][translation_id];
                    if (typeof(html) === "undefined" || !html.length) {
                        console.error("No translation for ID, lang: %s, %s", translation_id, lang);
                    } else {
                        $el.html(html);
                    }
                } else {
                    console.error("ID doesn't have a translation for the target lang: %s, %s", translation_id, lang);
                }
            } else {
                console.error("Target language not supported: %s", lang);
            }
        });
    }

    // Populate the english translation table
    $("[tid]").each(function (i, el) {
        var $el = $(el);
        translations.en[$el.attr("tid")] = $el.html();
    });

    // Set callbacks on hyper-links that trigger translation
    $("a[translate]").each(function (e, el) {
        var $el = $(el),
            lang = $el.attr("translate");

        if (lang in translations) {
            $el.click(function () {
                if (lang !== translation_lang) {
                    translate_to_lang(lang);
                }
            });
        } else {
            console.error("Language %s is not supported for translation", lang);
        }
    });

    // Remember the language selected across page loads
    var stored_lang = window.localStorage.getItem("translation-lang");
    if (stored_lang in translations && stored_lang !== translation_lang) {
        translate_to_lang(stored_lang);
    }
})();
