# Kompilace

---
Celá aplikace byla vyvíjena a kompilována na systému Ubuntu 20.04.

## Potřebné nástroje

### Cmake 3.17 nebo vyšší
### g++-10
### Vulkan SDK
Instalace pro Ubuntu 20.04

    wget -qO - https://packages.lunarg.com/lunarg-signing-key-pub.asc | sudo apt-key add -
    sudo wget -qO /etc/apt/sources.list.d/lunarg-vulkan-focal.list https://packages.lunarg.com/vulkan/lunarg-vulkan-focal.list
    sudo apt update
    sudo apt install vulkan-sdk

Pro jiné systémy, případně instalaci ze staženého balíčku, lze navštívit stránku https://vulkan.lunarg.com/ .

## Potřebné knihovny
* libglfw3-dev
* ffmpeg
* binutils-dev
* libavformat-dev
* libavcodec-dev
* libswscale-dev

## Kompilace
```bash
cd src
cmake -Bbuild -H. -DCMAKE_CXX_COMPILER="path_to_g++-10"
cmake --build build --target all
```
Tímto vznikne v adresáři `src/build` spustitelný soubor. 

# Spouštění

---

Program má jediný argument pro spuštění. Jedná se o cestu ke konfiguračnímu souboru. 
Pokud není soubor poskytnut pomocí argumentu, načítá se výchozí soubor `./config.toml`. 

    -c --config "path_to_config"

Pro správný běh programu je nutné mít správně vyplněné cesty v konfiguračním souboru.
Jedná se o parametry `pathToShaders`, `particleModel` a `cellModel`.
Výchozí hodnoty počítají se složkami `resources` a `shaders` na stejné úrovni, jako je složka se spustitelným souborem.
Program zároveň počítá s určitou strukturou těchto složek, takže po změně vnitřního uspořádání nebude program spustitelný.
# Ovládání

---

* Pohyb v prostoru pomocí šipek či kláves WSAD
* Pohyb kamery pomocí stisku levého tlačítka a pohybu myši

## GUI

### Hlavní Panel
Hlavní panel obsahuje často používané prvky.
Veškeré prvky jsou roztříděny do následujících skupin. 

#### Info
Jedná se pouze o informační panel zobrazující několik důležitých informací, jako je počet snímků za sekundu, krok simulace a jiné.

#### Ovládání simulace
Ovládání samotné simulace. 
Zde může uživatel spouštět a krokovat samotnou simulaci. 
Zároveň je zde možnost přepínat mezi jednotlivými druhy simulace a zobrazováním.

Poslední možností je uložení současného stavu simulace pro pozdější načtení, a to i po restartování programu. 
Jedná se o poměrně primitivní metodu ukládání obsahu bufferů na disk,
přičemž není kontrolováno, zda uložená data patří k danému scénáři, a proto může dojít k nestabilitě simulace i případnému pádu aplikace.

#### Nahrávání
Zde je možné pořídit záznam z probíhající simulace.
Pokud není vyplněn název/cesta souboru, je videosoubor vytvořen v aktuální složce s názvem `video.mp4`.
Nedochází k přepisování souborů a případné duplikáty dostávají pořadová čísla.
Pro nahrávání je podporován pouze kontejner mp4.

Je možné i uložit aktuální stav ve formě obrázku. 
Ty jsou pak vždy ukládány do aktuální složky, přičemž v názvu obsahují aktuální datum a čas.

#### Vizualizace
Zde je možné případně vizualizovat hustotu částic za pomocí barevného rozložení.


### Nastavení
K nastavení lze přistoupit pomocí horního menu v hlavním panelu. 
#### Nastavení simulace
Zde je možné měnit parametry simulace. Okno má záložky vztahující se k jednotlivým simulacím. 
Výchozí hodnoty jsou načítány z konfiguračních souborů. Při změně je důležité měnit i délku kroku simulace.
Některé kombinace hodnot vyžadují malý krok, jiné jsou stabilní i s větším krokem.
Změny se projeví až po uložení, při kterém zároveň dojde k resetování simulace.

#### Nastavení vykreslování
V tomto okně je možné měnit vizuální stránku. 
Je možné měnit pozici světla, ale i barvu kapaliny, či parametry algoritmu Marching cubes.

# Scénáře
K aplikaci je předpřipraveno několik scénářů/konfiguračních souborů.
Lze samozřejmě připravit několik dalších, nicméně je nutné mít na mysli, 
že některá kombinace parametrů nemusí produkovat správné výsledky.
Především je dobré si dávat pozor při zvýšené viskozitě či tuhosti plynu na dostatečně malý krok simulace.
Vypařování pak správně funguje pouze v prostoru o stejných rozměrech.

Předpřipravené scénáře obsahují testované a ve videu nahrané konfigurace. 
Jedná se o kolizi dvou mas kapalin, kapalinu v akváriu, vypařování. 
Jednotlivé konfigurace jsou poskytnuty ve více verzích, s rozdílným počtem částic.

