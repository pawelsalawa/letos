#ifndef UICONFIG_H
#define UICONFIG_H

#include "gui_global.h"
#include "config_builder.h"
#include "services/config.h"
#include <QFont>
#include <QHash>
#include <QColor>
#include <QTextCharFormat>

#define CFG_UI_CATEGORIES(Type,Body) CFG_CATEGORIES_EX(Type,GUI_API_EXPORT,Body)
#define CFG_UI_CATEGORY(Name,Body) CFG_CATEGORY_EX(Name,GUI_API_EXPORT,Body)

class QSplitter;
namespace Cfg
{
    GUI_API_EXPORT QVariant getStyleDefaultValue();
    GUI_API_EXPORT QVariant getDefaultTextEditorFont();
    GUI_API_EXPORT QVariant getDefaultItemViewFont();
    GUI_API_EXPORT QVariant getDefaultDbTreeLabelFont();
    GUI_API_EXPORT QVariant getDefaultSyntaxParenthesisBg();
    GUI_API_EXPORT QVariant getDefaultSyntaxParenthesisFg();
    GUI_API_EXPORT QVariant getDefaultSyntaxCurrentLineBg();
    GUI_API_EXPORT QVariant getDefaultSyntaxCurrentQueryBg();
    GUI_API_EXPORT QVariant getDefaultSyntaxValidObject();
    GUI_API_EXPORT QVariant getDefaultSyntaxForeground();
    GUI_API_EXPORT QVariant getDefaultSyntaxStringFg();
    GUI_API_EXPORT QVariant getDefaultSyntaxKeywordFg();
    GUI_API_EXPORT QVariant getDefaultSyntaxBindParamFg();
    GUI_API_EXPORT QVariant getDefaultSyntaxBlobFg();
    GUI_API_EXPORT QVariant getDefaultSyntaxCommentFg();
    GUI_API_EXPORT QVariant getDefaultSyntaxNumberFg();

    GUI_API_EXPORT QTextCharFormat getSyntaxParenthesisFormat();
    GUI_API_EXPORT QTextCharFormat getSyntaxCurrentLineFormat();
    GUI_API_EXPORT QTextCharFormat getSyntaxCurrentQueryFormat();
    GUI_API_EXPORT QTextCharFormat getSyntaxValidObjectFormat();
    GUI_API_EXPORT QTextCharFormat getSyntaxForegroundFormat();
    GUI_API_EXPORT QTextCharFormat getSyntaxStringFormat();
    GUI_API_EXPORT QTextCharFormat getSyntaxKeywordFormat();
    GUI_API_EXPORT QTextCharFormat getSyntaxBindParamFormat();
    GUI_API_EXPORT QTextCharFormat getSyntaxBlobFormat();
    GUI_API_EXPORT QTextCharFormat getSyntaxCommentFormat();
    GUI_API_EXPORT QTextCharFormat getSyntaxNumberFormat();
    GUI_API_EXPORT void handleSplitterState(QSplitter* splitter);

    typedef QHash<QString,QVariant> Session;
    typedef QHash<QString,QVariant> DataEditorsOrder;
    typedef QHash<QString,QVariant> ColorPickerConfig;
    typedef QHash<QString,QString> DataRenderers;
    typedef QHash<QString,QByteArray> SplitterStates;
    enum InsertRowPlacement
    {
        BEFORE_CURRENT = 0,
        AFTER_CURRENT = 1,
        AT_THE_END = 2
    };
    enum StatusFieldFadingMode
    {
        NO_FADE = 0,
        GRAY_OUT = 1,
        ERASE = 2
    };
}

CFG_UI_CATEGORIES(Ui,
    CFG_UI_CATEGORY(Fonts,
        CFG_ENTRY(QFont,        SqlEditor,               &Cfg::getDefaultTextEditorFont)
        CFG_ENTRY(QFont,        DataView,                &Cfg::getDefaultItemViewFont)
        CFG_ENTRY(QFont,        DbTree,                  &Cfg::getDefaultItemViewFont)
        CFG_ENTRY(QFont,        DbTreeLabel,             &Cfg::getDefaultDbTreeLabelFont)
        CFG_ENTRY(QFont,        StatusField,             &Cfg::getDefaultItemViewFont)
    )

    CFG_UI_CATEGORY(Colors,
        CFG_ENTRY(bool,   SyntaxParenthesisBgCustom,  false)
        CFG_ENTRY(QColor, SyntaxParenthesisBg,        &Cfg::getDefaultSyntaxParenthesisBg,  CFG_DEP(SyntaxParenthesisBgCustom))
        CFG_ENTRY(bool,   SyntaxParenthesisFgCustom,  false)
        CFG_ENTRY(QColor, SyntaxParenthesisFg,        &Cfg::getDefaultSyntaxParenthesisFg,  CFG_DEP(SyntaxParenthesisFgCustom))
        CFG_ENTRY(bool,   SyntaxParenthesisFgBold,    false,                                CFG_DEP(SyntaxParenthesisFgCustom))
        CFG_ENTRY(bool,   SyntaxParenthesisFgItalic,  false,                                CFG_DEP(SyntaxParenthesisFgCustom))
        CFG_ENTRY(bool,   SyntaxCurrentLineBgCustom,  false)
        CFG_ENTRY(QColor, SyntaxCurrentLineBg,        &Cfg::getDefaultSyntaxCurrentLineBg,  CFG_DEP(SyntaxCurrentLineBgCustom))
        CFG_ENTRY(bool,   SyntaxCurrentQueryBgCustom, false)
        CFG_ENTRY(QColor, SyntaxCurrentQueryBg,       &Cfg::getDefaultSyntaxCurrentQueryBg, CFG_DEP(SyntaxCurrentQueryBgCustom))
        CFG_ENTRY(bool,   SyntaxValidObjectCustom,    false)
        CFG_ENTRY(QColor, SyntaxValidObject,          &Cfg::getDefaultSyntaxValidObject,    CFG_DEP(SyntaxValidObjectCustom))
        CFG_ENTRY(bool,   SyntaxValidObjectBold,      false,                                CFG_DEP(SyntaxValidObjectCustom))
        CFG_ENTRY(bool,   SyntaxValidObjectItalic,    false,                                CFG_DEP(SyntaxValidObjectCustom))
        CFG_ENTRY(bool,   SyntaxForegroundCustom,     false)
        CFG_ENTRY(QColor, SyntaxForeground,           &Cfg::getDefaultSyntaxForeground,     CFG_DEP(SyntaxForegroundCustom))
        CFG_ENTRY(bool,   SyntaxForegroundBold,       false,                                CFG_DEP(SyntaxForegroundCustom))
        CFG_ENTRY(bool,   SyntaxForegroundItalic,     false,                                CFG_DEP(SyntaxForegroundCustom))
        CFG_ENTRY(bool,   SyntaxStringFgCustom,       false)
        CFG_ENTRY(QColor, SyntaxStringFg,             &Cfg::getDefaultSyntaxStringFg,       CFG_DEP(SyntaxStringFgCustom))
        CFG_ENTRY(bool,   SyntaxStringFgBold,         false,                                CFG_DEP(SyntaxStringFgCustom))
        CFG_ENTRY(bool,   SyntaxStringFgItalic,       true,                                 CFG_DEP(SyntaxStringFgCustom))
        CFG_ENTRY(bool,   SyntaxKeywordFgCustom,      false)
        CFG_ENTRY(QColor, SyntaxKeywordFg,            &Cfg::getDefaultSyntaxKeywordFg,      CFG_DEP(SyntaxKeywordFgCustom))
        CFG_ENTRY(bool,   SyntaxKeywordFgBold,        true,                                 CFG_DEP(SyntaxKeywordFgCustom))
        CFG_ENTRY(bool,   SyntaxKeywordFgItalic,      false,                                CFG_DEP(SyntaxKeywordFgCustom))
        CFG_ENTRY(bool,   SyntaxBindParamFgCustom,    false)
        CFG_ENTRY(QColor, SyntaxBindParamFg,          &Cfg::getDefaultSyntaxBindParamFg,    CFG_DEP(SyntaxBindParamFgCustom))
        CFG_ENTRY(bool,   SyntaxBindParamFgBold,      false,                                CFG_DEP(SyntaxBindParamFgCustom))
        CFG_ENTRY(bool,   SyntaxBindParamFgItalic,    false,                                CFG_DEP(SyntaxBindParamFgCustom))
        CFG_ENTRY(bool,   SyntaxBlobFgCustom,         false)
        CFG_ENTRY(QColor, SyntaxBlobFg,               &Cfg::getDefaultSyntaxBlobFg,         CFG_DEP(SyntaxBlobFgCustom))
        CFG_ENTRY(bool,   SyntaxBlobFgBold,           false,                                CFG_DEP(SyntaxBlobFgCustom))
        CFG_ENTRY(bool,   SyntaxBlobFgItalic,         false,                                CFG_DEP(SyntaxBlobFgCustom))
        CFG_ENTRY(bool,   SyntaxCommentFgCustom,      false)
        CFG_ENTRY(QColor, SyntaxCommentFg,            &Cfg::getDefaultSyntaxCommentFg,      CFG_DEP(SyntaxCommentFgCustom))
        CFG_ENTRY(bool,   SyntaxCommentFgBold,        false,                                CFG_DEP(SyntaxCommentFgCustom))
        CFG_ENTRY(bool,   SyntaxCommentFgItalic,      true,                                 CFG_DEP(SyntaxCommentFgCustom))
        CFG_ENTRY(bool,   SyntaxNumberFgCustom,       false)
        CFG_ENTRY(QColor, SyntaxNumberFg,             &Cfg::getDefaultSyntaxNumberFg,       CFG_DEP(SyntaxNumberFgCustom))
        CFG_ENTRY(bool,   SyntaxNumberFgBold,         false,                                CFG_DEP(SyntaxNumberFgCustom))
        CFG_ENTRY(bool,   SyntaxNumberFgItalic,       false,                                CFG_DEP(SyntaxNumberFgCustom))
    )

    CFG_UI_CATEGORY(DbList,
        CFG_ENTRY(bool,                    ExpandTables,                true)
        CFG_ENTRY(bool,                    ExpandViews,                 true)
        CFG_ENTRY(bool,                    ExpandSubNodes,              true)
        CFG_ENTRY(bool,                    SortObjects,                 true)
        CFG_ENTRY(bool,                    SortColumns,                 false)
        CFG_ENTRY(bool,                    ShowSystemObjects,           false)
        CFG_ENTRY(bool,                    ShowShadowTables,            false)
        CFG_ENTRY(bool,                    ShowDbTreeLabels,            true) // any labels at all
        CFG_ENTRY(bool,                    ShowRegularTableLabels,      false)
        CFG_ENTRY(bool,                    ShowVirtualTableLabels,      true)
        CFG_ENTRY(bool,                    ShowColumnTypeLabels,        true)
        CFG_ENTRY(bool,                    NewDbNotPermanentByDefault,  false)
        CFG_ENTRY(bool,                    BypassDbDialogWhenPossible,  true)
        CFG_ENTRY(bool,                    LinkWithMdiArea,             true)
    )

    CFG_UI_CATEGORY(General,
        CFG_ENTRY(QString,                 DataViewTabs,                QString())
        CFG_ENTRY(QString,                 SqlEditorTabs,               QString())
        CFG_ENTRY(int,                     SqlEditorExplainMode,        Cfg::EXPLAIN)
        CFG_ENTRY(QString,                 SqlEditorDbListOrder,        "LikeDbTree")
        CFG_ENTRY(bool,                    SqlEditorWrapWords,          false)
        CFG_ENTRY(bool,                    SqlEditorCurrQueryHighlight, true)
        CFG_ENTRY(qint64,                  SqlEditorExecTimePrecision,  6)
        CFG_ENTRY(bool,                    SqlEditorSnippetsIfEmpty,    true)
        CFG_ENTRY(bool,                    DisableBlinkingCursor,       false)
        CFG_ENTRY(bool,                    ExecuteCurrentQueryOnly,     true)
        CFG_ENTRY(bool,                    SqlEditorAutoCommit,         true)
        CFG_ENTRY(int,                     NumberOfRowsPerPage,         1000)
        CFG_ENTRY(bool,                    LimitRowsForManyColumns,     true)
        CFG_ENTRY(QString,                 Style,                       &Cfg::getStyleDefaultValue)
        CFG_ENTRY(QString,                 IconSet,                     QString())
        CFG_ENTRY(Cfg::Session,            Session,                     Cfg::Session())
        CFG_ENTRY(Cfg::SplitterStates,     SplitterStates,              Cfg::SplitterStates())
        CFG_ENTRY(bool,                    AllowMultipleSessions,       false)
        CFG_ENTRY(bool,                    RestoreSession,              true)
        CFG_ENTRY(bool,                    DontShowDdlPreview,          false)
        CFG_ENTRY(bool,                    OpenTablesOnData,            false)
        CFG_ENTRY(bool,                    DataTabAsFirstInTables,      false)
        CFG_ENTRY(bool,                    OpenViewsOnData,             false)
        CFG_ENTRY(bool,                    DataTabAsFirstInViews,       false)
        CFG_ENTRY(bool,                    AutoOpenStatusField,         true)
        CFG_ENTRY(int,                     StatusFieldMsgFadingMode,    Cfg::StatusFieldFadingMode::GRAY_OUT)
        CFG_ENTRY(Cfg::DataEditorsOrder,   DataEditorsOrder,            Cfg::DataEditorsOrder())
        CFG_ENTRY(Cfg::DataRenderers,      DataRenderers,               Cfg::DataRenderers())
        CFG_ENTRY(bool,                    DataEditAutoAdvanceOnEnter,  true)
        CFG_ENTRY(bool,                    DataEditAutoAdvanceEdit,     false,          CFG_DEP(DataEditAutoAdvanceOnEnter))
        CFG_ENTRY(QString,                 FileDialogLastPath,          QString())
        CFG_ENTRY(int,                     MaxInitialColumnWith,        600)
        CFG_ENTRY(bool,                    EnlargeColumnForValue,       true)
        CFG_ENTRY(bool,                    ColumnWidthForName,          false)
        CFG_ENTRY(bool,                    LanguageAsked,               false)
        CFG_ENTRY(bool,                    OpenMaximized,               true)
        CFG_ENTRY(QString,                 DockLayout,                  "vertical")
        CFG_ENTRY(QString,                 CustomCss,                   QString())
        CFG_ENTRY(bool,                    CompactLayout,               true)
        CFG_ENTRY(int,                     InsertRowPlacement,          Cfg::BEFORE_CURRENT)
        CFG_ENTRY(bool,                    ShowDataViewTooltips,        true)
        CFG_ENTRY(bool,                    KeepNullWhenEmptyValue,      true)
        CFG_ENTRY(bool,                    UseSciFormatForDoubles,      false)
        CFG_ENTRY(Cfg::ColorPickerConfig,  ColorPickerConfig,           Cfg::ColorPickerConfig())
        CFG_ENTRY(int,                     ToolBarIconSize,             0)
        CFG_ENTRY(QString,                 LastUsedConfigPage,          QString())
        CFG_ENTRY(bool,                    ShowPerColumnFilters,        false)
        CFG_ENTRY(bool,                    SingleColumnClickSort,       false)
        CFG_ENTRY(bool,                    WindowsInMenuBar,            true)
    )
)

GUI_API_EXPORT QString getFileDialogInitPath();
GUI_API_EXPORT void setFileDialogInitPath(const QString& path);
GUI_API_EXPORT void setFileDialogInitPathByFile(const QString& filePath);

#define CFG_UI CFG_INSTANCE(Ui)

#endif // UICONFIG_H
