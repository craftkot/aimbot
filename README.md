# Terraria Calamity × Fables — DUO Expanded

**Для двух игроков, tModLoader Stable (Terraria 1.4.4).** Список из **26 модов** и три варианта игры. Репозиторий подготовлен как установщик, манифест и кооперативные настройки.

| Профиль | Модов | Содержание |
|---|---:|---|
| \`recommended\` | 20 | Calamity + Fables + QoL; меньше дополнительных рисков |
| \`max\` | 26 | Все 26, включая Catalyst и Hunt of the Old God |
| \`fables\` | 20 | Fables без Calamity и без музыкальных модов |

## Установка (Windows)

1. Установить tModLoader в Steam, переключиться на **Stable** и запустить один раз.
2. Скачать ZIP этого репозитория через **Code → Download ZIP** и распаковать полностью.
3. Запустить **INSTALL_WINDOWS.cmd** (по умолчанию устанавливает профиль \`recommended\` и пробует скачать настоящие \`.tmod\` через **официальный SteamCMD**).
4. Для максимального контента: запустить \`INSTALL_WINDOWS.cmd -Profile max\`. Для Fables без музыкальных модов: \`INSTALL_WINDOWS.cmd -Profile fables\`.
5. Если SteamCMD сообщает о недоступности загрузок без авторизации, открыть **tModLoader → Workshop → Mod Packs**, выбрать профиль и воспользоваться загрузкой всех модов через Steam. **Пароль никому передавать не требуется.**
6. Второй игрок устанавливает тот же профиль и ту же версию tModLoader. Сравните версии модов, прежде чем подключаться.

Чтобы **не скачивать через SteamCMD**: \`INSTALL_WINDOWS.cmd -Method workshop\`. Для запуска без загрузки: \`INSTALL_WINDOWS.cmd -NoDownload\`.

### Что устанавливается автоматически

- Три папки в \`Documents\My Games\Terraria\tModLoader\ModPacks\` с \`Mods/install.txt\` и \`Mods/enabled.json\`.
- Список выбранных модов в \`Documents\My Games\Terraria\tModLoader\Mods\enabled.json\` **с резервной копией прежнего файла**.
- Если SteamCMD успешно скачает файлы, настоящие \`.tmod\` копируются в \`Mods\`. Неуспешные загрузки явно указываются.
- **Миры и персонажи не удаляются.** Старые одноимённые профили сохраняются с суффиксом \`_backup_\`.

### Важные ограничения

- Репозиторий **не содержит файлы чужих модов**: Calamity и Fables распространяются авторами через Workshop. Файлы могут быть большими, а открытый GitHub — неподходящее место для их перепубликации.
- SteamCMD может выдавать отказ на отдельные Workshop-элементы при анонимном входе; в этом случае официальный клиент tModLoader скачает их по манифесту.
- **Calamity Mod Music** в профилях \`recommended\` и \`max\` оставлен, поскольку в актуальных сборках он заявлен как зависимость Calamity. В \`fables\` музыкальных модов нет.
- Calamity + Fables — экспериментальное сочетание. Fables содержит функции совместимости, но **не гарантируется** корректная работа всех боссов, рецептов и мультиплеера.
- Сборка **не проходила реальный игровой тест**. Подготовлены проверяемые манифесты и установщик.

## Кооператив

Создайте новый **Large / Expert / Classic characters** мир **после включения модов**. В Expert оба получают личные мешки с наградами. Рекомендуемое распределение: стрелок + призыватель, или воин + маг. До запуска мира прочтите [гайд по настройкам](docs/COOP_GUIDE_RU.md).

**Исходники ссылок:** [каталог 26 модов](manifest/mods.json), [официальная инструкция tModLoader для ModPacks](https://docs.tmodloader.net/docs/stable/md__github_workspace_src_t_mod_loader__terraria_release_extras__dedicated_server_utils__r_e_a_d_m_e.html).


---

## NEW: NO CALAMITY DUO (40 / 52 / 39 mods)

- **40-mod recommended**: Thorium + standalone Calamity Fables + Consolaria + QoL.
- **52-mod epic**: more content, The Stars Above, Subworld Library, Vitality.
- **39-mod Thorium-only**: no Calamity Mod, no Fables.
- No standalone music mods in these profiles.
- Start **INSTALL_NO_CALAMITY.cmd** (optional `-Profile epic`, `-Profile thorium`, or `-Method workshop`).
- [Russian instructions](docs/NO_CALAMITY_DUO_RU.md).
- Workshop download in this installer is best-effort; download remaining mods via tModLoader if anonymous SteamCMD fails.
