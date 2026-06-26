# Grafika projekt "Swiat Spongeboba"

## Implementowane metody oswietlenia i renderowania

- Directional light: glowne swiatlo sceny ustawione przez `kLightDirection`.
- Shadow mapping: scena jest najpierw renderowana do mapy glebokosci z perspektywy swiatla, a potem shadery PBR/Toon probkuja `uShadowMap`.
- PBR shading: tryb materialowy z parametrami `metallic`, `roughness`, `ao`, kolorem bazowym i teksturami.
- Toon shading: uproszczone progowe cieniowanie kreskowkowe wlaczane z menu.
- Point light: dodatkowe swiatlo punktowe powiazane ze swiecaca meduza.
- Emission: wybrane obiekty, np. swiecaca meduza, moga emitowac kolor.
- Outline rendering: obiekty sa rysowane dodatkowym przebiegiem z odwracanym cullingiem, co tworzy kontur.
- Procedural skybox: podwodny cubemap generowany w kodzie, z efektem jasniejszej powierzchni, fal i caustics.

## Quest z meduzami

- Quest startuje przy Squidwardzie po nacisnieciu `SPACE`.
- Po starcie odtwarzany jest dzwiek `spongebob-task-start.mp3`.
- Po zakonczeniu dzwieku aktywuje sie 5 bialych meduz do zebrania.
- Meduzy znajduja sie w innej wiosce, dalej od centrum sceny.
- Gracz moze zebrac meduze, gdy jest blisko niej i nacisnie `SPACE`.
- Licznik questu pokazuje postep: `Bring 5 white jellyfish to Squidward. X/5`.
- Po zebraniu 5 meduz tekst zmienia sie na powrot do Squidwarda.
- Po powrocie do Squidwarda i nacisnieciu `ENTER` quest zostaje zakonczony.
- Przy zakonczeniu odtwarzany jest dzwiek zadania koncowego.

## Uruchomienie

Projekt jest budowany przez CMake/CLion.
