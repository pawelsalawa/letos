#ifndef TABLESTRUCTUREMODEL_H
#define TABLESTRUCTUREMODEL_H

#include "parser/ast/sqlitecreatetable.h"
#include "gui_global.h"
#include <QAbstractTableModel>
#include <QPointer>

class GUI_API_EXPORT TableStructureModel : public QAbstractTableModel
{
    Q_OBJECT

    public:
        explicit TableStructureModel(QObject *parent = 0);

        int rowCount(const QModelIndex& parent = QModelIndex()) const;
        int columnCount(const QModelIndex& parent = QModelIndex()) const;
        QVariant data(const QModelIndex& index, int role) const;
        QVariant headerData(int section, Qt::Orientation orientation, int role) const;
        Qt::DropActions supportedDropActions() const;
        Qt::DropActions supportedDragActions() const;
        QStringList mimeTypes() const;
        QMimeData* mimeData(const QModelIndexList& indexes) const;
        bool canDropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column, const QModelIndex& parent) const;
        bool dropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column, const QModelIndex& parent);
        Qt::ItemFlags flags(const QModelIndex& index) const;
        bool isModified() const;
        void setCreateTable(SqliteCreateTable* value);
        SqliteCreateTable::Column* getColumn(int colIdx) const;
        void replaceColumn(int colIdx, SqliteCreateTable::Column* column);
        void insertColumn(int colIdx, SqliteCreateTable::Column* column);
        void appendColumn(SqliteCreateTable::Column* column);
        void delColumn(int colIdx);
        void renameColumn(int colIdx, const QString& newName);
        void moveColumnUp(int colIdx);
        void moveColumnDown(int colIdx);
        void moveColumnTo(int colIdx, int newIdx);
        QModelIndex findColumn(const QString& columnName, Qt::CaseSensitivity cs = Qt::CaseSensitive) const;

        static bool hasTableStructureItem(const QMimeData* data);

    private:
        enum class Columns
        {
            NAME,
            TYPE,
            PK,
            FK,
            UNIQUE,
            CHECK,
            NOTNULL,
            COLLATE,
            GENERATED,
            DEFAULT
        };

        Columns getHeaderColumn(int colIdx) const;
        int getHeaderColumnIdx(Columns headerColumn) const;
        bool isValidColumnIdx(int colIdx) const;
        bool doesColumnHasConstraint(SqliteCreateTable::Column* column, SqliteCreateTable::Column::Constraint::Type type);
        bool isColumnHidden(int row) const;
        QString columnLabel(int column) const;
        QVariant getColumnName(int row) const;
        QVariant getColumnType(int row) const;
        QVariant getColumnPk(int row) const;
        QVariant getColumnFk(int row) const;
        QVariant getColumnUnique(int row) const;
        QVariant getColumnCheck(int row) const;
        QVariant getColumnNotNull(int row) const;
        QVariant getColumnCollate(int row) const;
        QVariant getColumnGenerate(int row) const;
        QVariant getColumnDefaultValue(int row) const;
        QVariant getColumnDefaultFont(int row) const;
        QVariant getColumnDefaultColor(int row) const;
        QVariant getColumnDefault(int row) const;
        bool isColumnPk(SqliteCreateTable::Column* column) const;
        bool isColumnFk(SqliteCreateTable::Column* column) const;
        bool isColumnUnique(SqliteCreateTable::Column* column) const;
        bool isColumnCheck(SqliteCreateTable::Column* column) const;
        bool isColumnNotNull(SqliteCreateTable::Column* column) const;
        bool isColumnCollate(SqliteCreateTable::Column* column) const;
        bool isColumnGenerate(SqliteCreateTable::Column* column) const;
        QString getToolTip(int row, TableStructureModel::Columns modelColumn) const;

        static const constexpr char* mimeType = "application/x-letos-tablestructuremodel-row-index";

        QPointer<SqliteCreateTable> createTable;
        bool modified = false;

    signals:
        void columnModified(const QString& oldColumn, SqliteCreateTable::Column* newColumn);
        void columnDeleted(const QString& column);
        void modifiyStateChanged();
        void columnsOrderChanged();
};

#endif // TABLESTRUCTUREMODEL_H
