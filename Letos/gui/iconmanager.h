#ifndef ICONMANAGER_H
#define ICONMANAGER_H

#include "icon.h"
#include "gui_global.h"
#include <QStringList>
#include <QHash>
#include <QIcon>
#include <QVariant>
#include <QFileInfo>
#include <QDir>

class QMovie;
class PluginType;
class Plugin;

class GUI_API_EXPORT IconManager : public QObject
{
        Q_OBJECT

    public:
        DEF_ICONS(Icons, iconEnums,
            DEF_ICON(ACT_ABORT,                         "act_abort")
            DEF_ICON(ACT_CLEAR,                         "act_clear")
            DEF_ICON(ACT_COPY,                          "act_copy")
            DEF_ICON(ACT_CUT,                           "act_cut")
            DEF_ICON(ACT_DEL_LINE,                      "act_del_line")
            DEF_ICON(ACT_DELETE,                        "act_delete")
            DEF_ICON(ACT_PASTE,                         "act_paste")
            DEF_ICON(ACT_REDO,                          "act_redo")
            DEF_ICON(ACT_SELECT_ALL,                    "act_select_all")
            DEF_ICON(ACT_UNDO,                          "act_undo")
            DEF_ICON(APPLY_FILTER,                      "apply_filter")
            DEF_ICON(APPLY_FILTER_RE,                   "apply_filter_re")
            DEF_ICON(APPLY_FILTER_SQL,                  "apply_filter_sql")
            DEF_ICON(APPLY_FILTER_TXT,                  "apply_filter_txt")
            DEF_ICON(APPLY_FILTER_TXT_STRICT,           "apply_filter_txt_strict")
            DEF_ICON(BUG,                               "bug")
            DEF_ICON(BUG_LIST,                          "bug_list")
            DEF_ICON(CANCEL,                            "cancel")
            DEF_ICON(CLEAR_HISTORY,                     "clear_history")
            DEF_ICON(CLEAR_LINEEDIT,                    "clear_lineedit")
            DEF_ICON(CLOSE,                             "close")
            DEF_ICON(CODE_ASSISTANT,                    "code_assistant")
            DEF_ICON(CODE_SNIPPET,                      "code_snippet")
            DEF_ICON(CODE_SNIPPET_ERROR,                "code_snippet_error")
            DEF_ICON(CODE_SNIPPETS,                     "code_snippets")
            DEF_ICON(CODE_SNIPPETS_EXPORT,              "code_snippets_export")
            DEF_ICON(CODE_SNIPPETS_IMPORT,              "code_snippets_import")
            DEF_ICON(COLLATIONS_EDITOR,                 "collations")
            DEF_ICON(COLLATIONS_EXPORT,                 "collations_export")
            DEF_ICON(COLLATIONS_IMPORT,                 "collations_import")
            DEF_ICON(COLUMN,                            "column")
            DEF_ICON(COLUMN_CONSTRAINT,                 "column_constraint")
            DEF_ICON(COLUMN_CONSTRAINT_ADD,             "column_constraint_add")
            DEF_ICON(COLUMN_CONSTRAINT_DEL,             "column_constraint_del")
            DEF_ICON(COLUMN_CONSTRAINT_EDIT,            "column_constraint_edit")
            DEF_ICON(COLUMNS,                           "columns")
            DEF_ICON(COMMAND_PALETTE,                   "command_palette")
            DEF_ICON(COMMIT,                            "commit")
            DEF_ICON(COMPLETE,                          "complete")
            DEF_ICON(COMPLETER_BLOB,                    "completer_blob")
            DEF_ICON(COMPLETER_NO_VALUE,                "completer_no_value")
            DEF_ICON(COMPLETER_NUMBER,                  "completer_number")
            DEF_ICON(COMPLETER_OPERATOR,                "completer_operator")
            DEF_ICO3(COMPLETER_OTHER,                   COMPLETER_NO_VALUE)
            DEF_ICON(COMPLETER_PRAGMA,                  "completer_pragma")
            DEF_ICON(COMPLETER_STRING,                  "completer_string")
            DEF_ICON(CONFIGURE,                         "configure")
            DEF_ICON(CONFIG_COLORS,                     "config_colors")
            DEF_ICON(CONFIG_DATA_EDITORS,               "config_data_editors")
            DEF_ICON(CONFIG_DATA_RENDERERS,             "config_data_renderers")
            DEF_ICON(CONFIG_DB_LIST,                    "config_dblist")
            DEF_ICON(CONFIG_EXPORT,                     "config_export")
            DEF_ICON(CONFIG_FONT,                       "config_font")
            DEF_ICON(CONFIG_GENERAL,                    "config_general")
            DEF_ICON(CONFIG_IMPORT,                     "config_import")
            DEF_ICON(CONFIG_KEYS,                       "config_keys")
            DEF_ICON(CONFIG_LOOK_AND_FEEL,              "config_look_and_feel")
            DEF_ICON(CONFIG_STYLE,                      "config_style")
            DEF_ICON(CONSTRAINT_CHECK,                  "check")
            DEF_ICON(CONSTRAINT_CHECK_ADD,              "check_plus")
            DEF_ICON(CONSTRAINT_COLLATION,              "collation")
            DEF_ICON(CONSTRAINT_COLLATION_ADD,          "collation_plus")
            DEF_ICON(CONSTRAINT_COLLATION_ERROR,        "collation_error")
            DEF_ICON(CONSTRAINT_DEFAULT,                "default")
            DEF_ICON(CONSTRAINT_DEFAULT_ADD,            "default_plus")
            DEF_ICON(CONSTRAINT_FOREIGN_KEY,            "fk")
            DEF_ICON(CONSTRAINT_FOREIGN_KEY_ADD,        "fk_plus")
            DEF_ICON(CONSTRAINT_GENERATED,              "generated")
            DEF_ICON(CONSTRAINT_GENERATED_STORED,       "generated_disk")
            DEF_ICON(CONSTRAINT_GENERATED_VIRTUAL,      "generated_lightening")
            DEF_ICON(CONSTRAINT_GENERATED_ADD,          "generated_plus")
            DEF_ICON(CONSTRAINT_NOT_NULL,               "not_null")
            DEF_ICON(CONSTRAINT_NOT_NULL_ADD,           "not_null_plus")
            DEF_ICON(CONSTRAINT_PRIMARY_KEY,            "pk")
            DEF_ICON(CONSTRAINT_PRIMARY_KEY_ADD,        "pk_plus")
            DEF_ICON(CONSTRAINT_UNIQUE,                 "unique")
            DEF_ICON(CONSTRAINT_UNIQUE_ADD,             "unique_plus")
            DEF_ICON(VACUUM_DB,                         "vacuum_db")
            DEF_ICON(INTEGRITY_CHECK,                   "integrity_check")
            DEF_ICON(DATABASE,                          "database")
            DEF_ICON(DATA_DELETE,                       "data_delete")
            DEF_ICON(DATA_INSERT,                       "data_insert")
            DEF_ICON(DATA_SELECT,                       "data_select")
            DEF_ICON(DATA_UPDATE,                       "data_update")
            DEF_ICON(DATABASE_ADD,                      "database_add")
            DEF_ICON(DATABASE_CONNECT,                  "database_connect")
            DEF_ICON(DATABASE_DEL,                      "database_del")
            DEF_ICON(DATABASE_DISCONNECT,               "database_disconnect")
            DEF_ICON(DATABASE_EDIT,                     "database_edit")
            DEF_ICON(DATABASE_EXPORT,                   "database_export")
            DEF_ICON(DATABASE_IMPORT,                   "database_import")
            DEF_ICON(DATABASE_NEW,                      "database_new")
            DEF_ICON(DATABASE_OFFLINE,                  "database_offline")
            DEF_ICON(DATABASE_OPEN,                     "database_open")
            DEF_ICON(DATABASE_INVALID,                  "database_invalid")
            DEF_ICO3(DATABASE_ONLINE,                   DATABASE)
            DEF_ICON(DATABASE_RELOAD,                   "database_reload")
            DEF_ICON(DATABASE_RENAME,                   "database_rename")
            DEF_ICON(DDL_HISTORY,                       "ddl_history")
            DEF_ICON(DELETE,                            "delete")
            DEF_ICON(DELETE_ROW,                        "delete_row")
            DEF_ICO3(DELETE_COLLATION,                  DELETE_ROW)
            DEF_ICO3(DELETE_DATATYPE,                   DELETE_ROW)
            DEF_ICO3(DELETE_FN_ARG,                     DELETE_ROW)
            DEF_ICO3(DELETE_FUNCTION,                   DELETE_ROW)
            DEF_ICON(DELETE_SELECTED,                   "delete_selected")
            DEF_ICON(DIRECTORY,                         "directory")
            DEF_ICON(DIRECTORY_ADD,                     "directory_add")
            DEF_ICON(DIRECTORY_DEL,                     "directory_delete")
            DEF_ICON(DIRECTORY_EDIT,                    "directory_edit")
            DEF_ICON(DIRECTORY_OPEN,                    "directory_open")
            DEF_ICON(DIRECTORY_OPEN_WITH_DB,            "directory_open_with_db")
            DEF_ICON(DOCK_LAYOUT_HORIZONTAL,            "dock_layout_horizontal")
            DEF_ICON(DOCK_LAYOUT_VERTICAL,              "dock_layout_vertical")
            DEF_ICON(DONATE,                            "donate")
            DEF_ICON(ERASE,                             "erase")
            DEF_ICON(ERASE_TABLE_DATA,                  "erase_table_data")
            DEF_ICON(EXEC_QUERY,                        "exec_query")
            DEF_ICON(EXEC_SQL_FROM_FILE,                "execute_sql_from_file")
            DEF_ICON(EXPLAIN_QUERY,                     "explain_query")
            DEF_ICON(EXPORT,                            "export")
            DEF_ICON(EXPORT_FILE_BROWSE,                "export_file_browse")
            DEF_ICON(EXPORT_SQL_HISTORY,                "export_sql_history")
            DEF_ICON(EXTENSION,                         "brick")
            DEF_ICON(EXTENSION_ADD,                     "brick_add")
            DEF_ICON(EXTENSION_DELETE,                  "brick_delete")
            DEF_ICON(EXTENSION_IMPORT,                  "extensions_import")
            DEF_ICON(EXTENSION_ERROR,                   "brick_error")
            DEF_ICON(EXTENSION_EDITOR,                  "extensions")
            DEF_ICON(EXTENSION_EXPORT,                  "extensions_export")
            DEF_ICON(EYE_OPEN,                          "eye_open")
            DEF_ICON(EYE_CLOSED,                        "eye_closed")
            DEF_ICON(FEATURE_REQUEST,                   "feature_request")
            DEF_ICON(FONT_BROWSE,                       "font_browse")
            DEF_ICON(FORMAT_SQL,                        "format_sql")
            DEF_ICON(FUNCTION,                          "function")
            DEF_ICON(FUNCTION_ERROR,                    "function_error")
            DEF_ICON(FUNCTIONS_EDITOR,                  "functions")
            DEF_ICON(FUNCTIONS_EXPORT,                  "functions_export")
            DEF_ICON(FUNCTIONS_IMPORT,                  "functions_import")
            DEF_ICON(GET_UPDATE,                        "get_update")
            DEF_ICON(GO_BACK,                           "go_back")
            DEF_ICON(GENERATE_COLUMNS,                  "generate_columns")
            DEF_ICO3(GENERATE_QUERY,                    GENERATE_COLUMNS)
            DEF_ICON(HELP,                              "help")
            DEF_ICON(HIDDEN_COLUMN,                     "hidden_column")
            DEF_ICON(HOMEPAGE,                          "homepage")
            DEF_ICON(IMPORT,                            "import")
            DEF_ICON(INDEX,                             "index")
            DEF_ICON(INDEX_ADD,                         "index_add")
            DEF_ICON(INDEX_DEL,                         "index_delete")
            DEF_ICON(INDEX_EDIT,                        "index_edit")
            DEF_ICON(INDEX_RENAME,                      "rename_index")
            DEF_ICON(INDEX_EXPR_ADD,                    "index_hash_add")
            DEF_ICON(INDEX_EXPR_EDIT,                   "index_hash_edit")
            DEF_ICON(INDEX_EXPR_DEL,                    "index_hash_delete")
            DEF_ICON(INDEXES,                           "indexes")
            DEF_ICON(INDICATOR_ERROR,                   "indicator_error")
            DEF_ICON(INDICATOR_HINT,                    "indicator_hint")
            DEF_ICON(INDICATOR_INFO,                    "indicator_info")
            DEF_ICON(INDICATOR_WARN,                    "indicator_warn")
            DEF_ICON(INSERT_ROW,                        "insert_row")
            DEF_ICON(INSERT_ROWS,                       "insert_rows")
            DEF_ICO3(INSERT_FN_ARG,                     INSERT_ROW)
            DEF_ICO3(INSERT_DATATYPE,                   INSERT_ROW)
            DEF_ICON(KEYWORD,                           "keyword")
            DEF_ICON(LICENSES,                          "licenses")
            DEF_ICON(LINK,                              "link")
            DEF_ICO3(LIST_ITEM_OTHER,                   COMPLETER_NO_VALUE)
            DEF_ICON(LOADING,                           "loading")
            DEF_ICON(MINIFY,                            "minify")
            DEF_ICON(MOVE_DOWN,                         "move_down")
            DEF_ICON(MOVE_UP,                           "move_up")
            DEF_ICO3(NEW_COLLATION,                     INSERT_ROW)
            DEF_ICO3(NEW_FUNCTION,                      INSERT_ROW)
            DEF_ICON(OK,                                "ok")
            DEF_ICO3(OPEN_FILE,                         EXEC_SQL_FROM_FILE)
            DEF_ICON(OPEN_SQL_EDITOR,                   "open_sql_editor")
            DEF_ICO3(OPEN_SQL_FILE,                     EXEC_SQL_FROM_FILE)
            DEF_ICON(OPEN_VALUE_EDITOR,                 "open_value_editor")
            DEF_ICON(PAGE_FIRST,                        "page_first")
            DEF_ICON(PAGE_LAST,                         "page_last")
            DEF_ICON(PAGE_NEXT,                         "page_next")
            DEF_ICON(PAGE_PREV,                         "page_prev")
            DEF_ICO3(MOVE_LEFT,                         PAGE_PREV)
            DEF_ICO3(MOVE_RIGHT,                        PAGE_NEXT)
            DEF_ICON(PLUGIN,                            "plugin")
            DEF_ICON(PLUS,                              "plus")
            DEF_ICON(QUIT,                              "quit")
            DEF_ICON(RELOAD,                            "reload")
            DEF_ICON(RENAME_BUTTON,                     "rename_button")
            DEF_ICO3(RENAME_FN_ARG,                     RENAME_BUTTON)
            DEF_ICO3(RENAME_DATATYPE,                   RENAME_BUTTON)
            DEF_ICON(RESET_AUTOINCREMENT,               "reset_autoincrement")
            DEF_ICO3(RESTORE_DEFAULT,                   ACT_UNDO)
            DEF_ICON(RESULTS_BELOW,                     "results_below")
            DEF_ICON(RESULTS_IN_TAB,                    "results_in_tab")
            DEF_ICON(RESULTS_ON_RIGHT,                  "results_on_right")
            DEF_ICON(ROLLBACK,                          "rollback")
            DEF_ICON(SAVE_FILE,                         "save_file")
            DEF_ICO3(SAVE_SQL_FILE,                     SAVE_FILE)
            DEF_ICON(SEARCH,                            "search")
            DEF_ICON(SELECTION_INVERT,                  "act_selection_invert")
            DEF_ICON(SET_NULL,                          "set_null")
            DEF_ICON(SORT_COLUMNS,                      "sort_columns")
            DEF_ICON(SORT_COUNT_01_ASC,                 "sort_cnt_01_asc")
            DEF_ICON(SORT_COUNT_01_DESC,                "sort_cnt_01_desc")
            DEF_ICON(SORT_COUNT_02_ASC,                 "sort_cnt_02_asc")
            DEF_ICON(SORT_COUNT_02_DESC,                "sort_cnt_02_desc")
            DEF_ICON(SORT_COUNT_03_ASC,                 "sort_cnt_03_asc")
            DEF_ICON(SORT_COUNT_03_DESC,                "sort_cnt_03_desc")
            DEF_ICON(SORT_COUNT_04_ASC,                 "sort_cnt_04_asc")
            DEF_ICON(SORT_COUNT_04_DESC,                "sort_cnt_04_desc")
            DEF_ICON(SORT_COUNT_05_ASC,                 "sort_cnt_05_asc")
            DEF_ICON(SORT_COUNT_05_DESC,                "sort_cnt_05_desc")
            DEF_ICON(SORT_COUNT_06_ASC,                 "sort_cnt_06_asc")
            DEF_ICON(SORT_COUNT_06_DESC,                "sort_cnt_06_desc")
            DEF_ICON(SORT_COUNT_07_ASC,                 "sort_cnt_07_asc")
            DEF_ICON(SORT_COUNT_07_DESC,                "sort_cnt_07_desc")
            DEF_ICON(SORT_COUNT_08_ASC,                 "sort_cnt_08_asc")
            DEF_ICON(SORT_COUNT_08_DESC,                "sort_cnt_08_desc")
            DEF_ICON(SORT_COUNT_09_ASC,                 "sort_cnt_09_asc")
            DEF_ICON(SORT_COUNT_09_DESC,                "sort_cnt_09_desc")
            DEF_ICON(SORT_COUNT_10_ASC,                 "sort_cnt_10_asc")
            DEF_ICON(SORT_COUNT_10_DESC,                "sort_cnt_10_desc")
            DEF_ICON(SORT_COUNT_11_ASC,                 "sort_cnt_11_asc")
            DEF_ICON(SORT_COUNT_11_DESC,                "sort_cnt_11_desc")
            DEF_ICON(SORT_COUNT_12_ASC,                 "sort_cnt_12_asc")
            DEF_ICON(SORT_COUNT_12_DESC,                "sort_cnt_12_desc")
            DEF_ICON(SORT_COUNT_13_ASC,                 "sort_cnt_13_asc")
            DEF_ICON(SORT_COUNT_13_DESC,                "sort_cnt_13_desc")
            DEF_ICON(SORT_COUNT_14_ASC,                 "sort_cnt_14_asc")
            DEF_ICON(SORT_COUNT_14_DESC,                "sort_cnt_14_desc")
            DEF_ICON(SORT_COUNT_15_ASC,                 "sort_cnt_15_asc")
            DEF_ICON(SORT_COUNT_15_DESC,                "sort_cnt_15_desc")
            DEF_ICON(SORT_COUNT_16_ASC,                 "sort_cnt_16_asc")
            DEF_ICON(SORT_COUNT_16_DESC,                "sort_cnt_16_desc")
            DEF_ICON(SORT_COUNT_17_ASC,                 "sort_cnt_17_asc")
            DEF_ICON(SORT_COUNT_17_DESC,                "sort_cnt_17_desc")
            DEF_ICON(SORT_COUNT_18_ASC,                 "sort_cnt_18_asc")
            DEF_ICON(SORT_COUNT_18_DESC,                "sort_cnt_18_desc")
            DEF_ICON(SORT_COUNT_19_ASC,                 "sort_cnt_19_asc")
            DEF_ICON(SORT_COUNT_19_DESC,                "sort_cnt_19_desc")
            DEF_ICON(SORT_COUNT_20_ASC,                 "sort_cnt_20_asc")
            DEF_ICON(SORT_COUNT_20_DESC,                "sort_cnt_20_desc")
            DEF_ICON(SORT_COUNT_20_PLUS_ASC,            "sort_cnt_20p_asc")
            DEF_ICON(SORT_COUNT_20_PLUS_DESC,           "sort_cnt_20p_desc")
            DEF_ICON(SORT_RESET,                        "sort_reset")
            DEF_ICON(SQLITE_DOCS,                       "sqlite_docs")
            DEF_ICON(SQL_EDITOR_SETTINGS,               "sql_editor_settings")
            DEF_ICON(LETOS_APP,                         "letos")
            DEF_ICON(LETOS_APP16,                       "letos_16")
            DEF_ICON(STATUS_ERROR,                      "status_error")
            DEF_ICON(STATUS_INFO,                       "status_info")
            DEF_ICON(STATUS_WARNING,                    "status_warning")
            DEF_ICON(TABLE,                             "table")
            DEF_ICON(TABLE_ADD,                         "table_add")
            DEF_ICON(TABLE_COLUMN_ADD,                  "table_column_add")
            DEF_ICON(TABLE_COLUMN_DELETE,               "table_column_delete")
            DEF_ICON(TABLE_COLUMN_EDIT,                 "table_column_edit")
            DEF_ICON(TABLE_COLUMN_RENAME,               "rename_column")
            DEF_ICON(TABLE_CONSTRAINT,                  "table_constraint")
            DEF_ICON(TABLE_CONSTRAINT_ADD,              "table_constraint_add")
            DEF_ICON(TABLE_CONSTRAINT_DELETE,           "table_constraint_delete")
            DEF_ICON(TABLE_CONSTRAINT_EDIT,             "table_constraint_edit")
            DEF_ICON(TABLE_CREATE_SIMILAR,              "table_create_similar")
            DEF_ICON(TABLE_DEL,                         "table_delete")
            DEF_ICON(TABLE_EDIT,                        "table_edit")
            DEF_ICON(TABLE_RENAME,                      "rename_table")
            DEF_ICON(TABLE_EXPORT,                      "table_export")
            DEF_ICON(TABLE_IMPORT,                      "table_import")
            DEF_ICON(TABLE_POPULATE,                    "table_populate")
            DEF_ICON(TABLES,                            "tables")
            DEF_ICON(TABS_AT_BOTTOM,                    "tabs_at_bottom")
            DEF_ICON(TABS_ON_TOP,                       "tabs_on_top")
            DEF_ICON(TEST_CONN_ERROR,                   "test_conn_error")
            DEF_ICON(TEST_CONN_OK,                      "test_conn_ok")
            DEF_ICON(TRIGGER,                           "trigger")
            DEF_ICON(TRIGGER_ADD,                       "trigger_add")
            DEF_ICON(TRIGGER_COLUMNS,                   "trigger_columns")
            DEF_ICON(TRIGGER_COLUMNS_INVALID,           "trigger_columns_error")
            DEF_ICON(TRIGGER_DEL,                       "trigger_delete")
            DEF_ICON(TRIGGER_EDIT,                      "trigger_edit")
            DEF_ICON(TRIGGER_RENAME,                    "rename_trigger")
            DEF_ICON(TRIGGERS,                          "triggers")
            DEF_ICON(TX_APPLY,                          "tx_apply")
            DEF_ICON(TX_DISCARD,                        "tx_discard")
            DEF_ICON(USER_MANUAL,                       "user_manual")
            DEF_ICON(UNLINK,                            "unlink")
            DEF_ICON(VIEW,                              "view")
            DEF_ICON(VIEW_ADD,                          "view_add")
            DEF_ICON(VIEW_DEL,                          "view_delete")
            DEF_ICON(VIEW_EDIT,                         "view_edit")
            DEF_ICON(VIEW_EXPORT,                       "view_export")
            DEF_ICON(VIEW_RENAME,                       "rename_view")
            DEF_ICON(VIEWS,                             "views")
            DEF_ICON(SHADOW_TABLE,                      "shadow_table")
            DEF_ICON(VIRTUAL_TABLE,                     "virtual_table")
            DEF_ICON(WIN_CASCADE,                       "win_cascade")
            DEF_ICON(WIN_TILE,                          "win_tile")
            DEF_ICON(WIN_TILE_HORIZONTAL,               "win_tile_horizontal")
            DEF_ICON(WIN_TILE_VERTICAL,                 "win_tile_vertical")
            DEF_ICON(WIN_CLOSE,                         "window_close")
            DEF_ICON(WIN_CLOSE_ALL,                     "window_close_all")
            DEF_ICON(WIN_CLOSE_ALL_LEFT,                "window_close_all_left")
            DEF_ICON(WIN_CLOSE_ALL_RIGHT,               "window_close_all_right")
            DEF_ICON(WIN_CLOSE_OTHER,                   "window_close_other")
            DEF_ICON(WIN_RESTORE,                       "window_restore")
            DEF_ICON(WIN_RENAME,                        "window_rename")
            DEF_ICON(ZOOM_RESET,                        "zoom")
            DEF_ICON(ZOOM_IN,                           "zoom_in")
            DEF_ICON(ZOOM_OUT,                          "zoom_out")
        )

        struct IconSetMetadata
        {
            QString dirPath;
            QString name;
            QString author;
            QString license;
            QString url;
            QString description;
            QList<QIcon> sampleIcons;
        };

        static IconManager* getInstance();

        QString getFilePathForName(const QString& name);
        bool isMovie(const QString& name);
        QMovie* getMoviePtr(const QString& name);

        /**
         * @brief Provides icon from either currently selected iconset or default iconset.
         * @param name Name of the icon (icon file base name, i.e. excluding dot and extension).
         * @param fallbackPath Optional full path to the icon file to use if the icon is not found in the current iconset.
         *        If not provided, will return a null QIcon. Useful if plugin uses it's own icons.
         * @return QIcon object for the requested icon, or a null QIcon if not found and no fallback path is provided.
         */
        QIcon getIcon(const QString& name, const QString& fallbackPath = QString());
        bool isSvg(const QString& name) const;
        void init();
        QStringList getIconDirs() const;
        const QHash<QString, IconSetMetadata>& getAvailableIconSets() const;
        IconSetMetadata& getIconSet(const QString& key);

    private:
        IconManager();
        void scanIconSets(const QString& dirPath);
        void scanIconSet(const QString& dirPath);
        void loadRecurently(QString dirPath, const QString& prefix, bool onlyNew);
        void loadRecurently(QString dirPath, const QString& prefix, bool movie, bool onlyNew);
        void enableRescanning();
        QString getPathForCurrentIconSet(const QFileInfo& entry) const;
        void initIconSets();
        void loadSampleIconsToIconSet(const QDir& dir, IconSetMetadata& iconSet);

        static IconManager* instance;
        QHash<QString,QIcon> icons;
        QList<const Icon*> sampleIconSetIcons;
        QHash<QString,QMovie*> movies;
        QHash<QString,QString> paths;
        QHash<QString,bool> svgs;
        QStringList iconDirs;
        QStringList iconFileExtensions;
        QStringList movieFileExtensions;
        QStringList iconExtensionOnlyList;
        QStringList resourceIcons;
        QStringList resourceMovies;
        QHash<QString, QStringList> pluginResourceIcons;
        QHash<QString, QStringList> pluginResourceMovies;
        QHash<QString, IconSetMetadata> iconSets;
        QString currentIconSet;
        QDir currentIconSetDir;

    private slots:
        void rescanResources(Plugin* plugin, PluginType* pluginType);
        void pluginsAboutToMassUnload();

    public slots:
        void rescanResources(const QString& pluginName = QString());

    signals:
        void rescannedFor(const QString& pluginName);
};

#define ICONMANAGER IconManager::getInstance()
#define ICONS ICONMANAGER->iconEnums

#endif // ICONMANAGER_H
