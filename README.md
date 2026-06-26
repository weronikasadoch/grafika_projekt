# Projekt z grafiki komputerowej: "Świat SpongeBoba"

## Zaimplementowane metody oświetlenia i renderowania

- Directional light: główne światło sceny ustawione za pomocą `kLightDirection`.
- Shadow mapping: scena jest najpierw renderowana do mapy głębokości z perspektywy światła, a następnie shadery PBR/Toon korzystają z `uShadowMap`.
- PBR shading: tryb materiałowy z parametrami `metallic`, `roughness`, `ao`, kolorem bazowym i teksturami.
- Toon shading: uproszczone cieniowanie kreskówkowe z progami jasności, włączane z poziomu menu.
- Point light: dodatkowe światło powiązane ze świecącą meduzą.
- Emission: wybrane obiekty, np. świecąca meduza, mogą emitować kolor.
- Outline rendering: obiekty są renderowane dodatkowym przebiegiem z odwróconym cullingiem, co tworzy efekt obrysu.
- Procedural skybox: podwodny cubemap generowany w kodzie, z efektem jaśniejszej powierzchni, fal i kaustyki.

## Quest z meduzami

- Quest rozpoczyna się przy Squidwardzie po naciśnięciu `SPACE`.
- Po rozpoczęciu zadania odtwarzany jest dźwięk `spongebob-task-start.mp3`.
- Po zakończeniu dźwięku aktywuje się 5 białych meduz do zebrania.
- Meduzy znajdują się w innej wiosce, dalej od centrum sceny.
- Gracz może zebrać meduzę, gdy znajduje się blisko niej i naciśnie `SPACE`.
- Licznik questu pokazuje postęp: `Bring 5 white jellyfish to Squidward. X/5`.
- Po zebraniu 5 meduz tekst zmienia się na informację o powrocie do Squidwarda.
- Po powrocie do Squidwarda i naciśnięciu `ENTER` quest zostaje zakończony.
- Po zakończeniu zadania odtwarzany jest dźwięk końcowy.

## Uruchomienie

Projekt jest budowany przez CMake/CLion.
