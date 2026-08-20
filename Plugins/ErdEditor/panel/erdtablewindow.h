#ifndef ERDTABLEWINDOW_H
#define ERDTABLEWINDOW_H

#include "windows/tablewindow.h"
#include "erdpropertiespanel.h"
#include <QObject>

class ErdEntity;
class ErdChange;

class ErdTableWindow : public TableWindow, public ErdPropertiesPanel
{
        Q_OBJECT

    public:
        ErdTableWindow(Db* db, ErdEntity* entity, QWidget* parent = nullptr);
        ~ErdTableWindow();

        QString getQuitUncommittedConfirmMessage() const override;
        bool commitErdChange() override;
        void abortErdChange() override;
        bool editColumn(const QString& columnName);
        ErdEntity* getEntity() const;
        void init() override;

    protected:
        bool resolveOriginalCreateTableStatement() override;
        bool resolveCreateTableStatement() override;
        void applyInitialTab();
        void defineCurrentContextDb() override;

    private:
        bool handleFailedStructureChanges(bool skipWarning);
        void setErrorRecording(bool enabled);

        ErdEntity* entity = nullptr;
        QStringList recordedErrors;
        QString originalContent;

    public slots:
        void changesSuccessfullyCommitted();
        bool commitStructure(bool skipWarning = false) override;
        void rollbackStructure() override;
        void nameEditedInline(const QString& newName);
        void columnEditedInline(int columnIdx, const QString& newName);
        void columnDeletedInline(int columnIdx);

    protected slots:
        void executeStructureChanges() override;
        void errorRecorded(const QString& msg);

    signals:
        void changeCreated(ErdChange* change);
        void editedEntityShouldBeDeleted(ErdEntity* entity);
        void requestReEditForEntity(ErdEntity* entity);
};

#endif // ERDTABLEWINDOW_H
