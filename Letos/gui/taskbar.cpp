#include "taskbar.h"
#include "mainwindow.h"
#include "uiutils.h"
#include "uiconfig.h"
#include <QMouseEvent>
#include <QMimeData>
#include <QDataStream>
#include <QDrag>
#include <QToolButton>
#include <QCursor>
#include <QAction>
#include <QStyle>
#include <QRubberBand>
#include <QApplication>
#include <QMenu>
#include <QResizeEvent>
#include <climits>

/**
 * @brief Container for task buttons used when the TaskBar is in the wrapping mode.
 *
 * QToolBar cannot wrap its items into multiple lines, so in the wrapping mode the TaskBar
 * holds a single instance of this widget and the task buttons live inside of it.
 * Buttons are laid out in lines along the toolbar orientation (rows for a horizontal toolbar,
 * columns for a vertical one) and the widget reports a size hint that makes the toolbar
 * as thick as needed to show all lines.
 */
class TaskBarFlowWidget : public QWidget
{
    public:
        explicit TaskBarFlowWidget(QToolBar* toolBar);

        void insertButton(int index, QToolButton* btn);
        void removeButton(QToolButton* btn);
        void moveButton(QToolButton* btn, int index);
        void setFlow(Qt::Orientation value);

        QSize sizeHint() const override;
        QSize minimumSizeHint() const override;

    protected:
        bool event(QEvent* e) override;
        void resizeEvent(QResizeEvent* e) override;

    private:
        QSize itemSize(QToolButton* btn) const;
        int spacing() const;

        /**
         * @brief Calculates layout of buttons.
         * @param available Space available along the flow direction.
         * @param geometries If not null, gets filled with geometries of buttons, in order of the buttons list.
         * @return Size occupied by buttons, where width is along the flow direction and height is perpendicular to it.
         */
        QSize calculate(int available, QList<QRect>* geometries = nullptr) const;
        void relayout();

        QToolBar* toolBar = nullptr;
        QList<QToolButton*> buttons;
        Qt::Orientation flow = Qt::Horizontal;
        int perpendicularExtent = 0;
};

TaskBarFlowWidget::TaskBarFlowWidget(QToolBar* toolBar) :
    QWidget(toolBar), toolBar(toolBar)
{
    setFlow(toolBar->orientation());
}

void TaskBarFlowWidget::insertButton(int index, QToolButton* btn)
{
    if (index < 0 || index > buttons.size())
        index = buttons.size();

    btn->setParent(this);
    buttons.insert(index, btn);
    btn->show();
    relayout();
    updateGeometry();
}

void TaskBarFlowWidget::removeButton(QToolButton* btn)
{
    buttons.removeOne(btn);
    relayout();
    updateGeometry();
}

void TaskBarFlowWidget::moveButton(QToolButton* btn, int index)
{
    int oldIdx = buttons.indexOf(btn);
    if (oldIdx < 0 || index < 0 || index >= buttons.size() || oldIdx == index)
        return;

    buttons.move(oldIdx, index);
    relayout();
    updateGeometry();
}

void TaskBarFlowWidget::setFlow(Qt::Orientation value)
{
    flow = value;
    if (flow == Qt::Horizontal)
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    else
        setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);

    perpendicularExtent = 0;
    relayout();
    updateGeometry();
}

QSize TaskBarFlowWidget::sizeHint() const
{
    // Along the flow: as much as needed to put all buttons in a single line (just like the QToolBar would ask for).
    // Perpendicular to the flow: as much as needed for all lines at the current size.
    QSize singleLine = calculate(INT_MAX);
    int perp = perpendicularExtent > 0 ? perpendicularExtent : singleLine.height();
    return (flow == Qt::Horizontal) ? QSize(singleLine.width(), perp) : QSize(perp, singleLine.width());
}

QSize TaskBarFlowWidget::minimumSizeHint() const
{
    // Minimum is a single line with a single button. Additional lines are requested only by the sizeHint(),
    // so they never enlarge the minimum size of the main window (which would make the window grow).
    int maxAlong = 0;
    int minPerp = 0;
    for (QToolButton* btn : buttons)
    {
        QSize s = itemSize(btn);
        maxAlong = qMax(maxAlong, (flow == Qt::Horizontal) ? s.width() : s.height());
        minPerp = qMax(minPerp, (flow == Qt::Horizontal) ? s.height() : s.width());
    }

    return (flow == Qt::Horizontal) ? QSize(maxAlong, minPerp) : QSize(minPerp, maxAlong);
}

bool TaskBarFlowWidget::event(QEvent* e)
{
    if (e->type() == QEvent::LayoutRequest)
    {
        // One of buttons changed its size hint (i.e. window was renamed, or icon size was changed).
        relayout();
        updateGeometry();
        return true;
    }
    return QWidget::event(e);
}

void TaskBarFlowWidget::resizeEvent(QResizeEvent* e)
{
    QWidget::resizeEvent(e);
    int oldExtent = perpendicularExtent;
    relayout();
    if (perpendicularExtent != oldExtent)
        updateGeometry();
}

QSize TaskBarFlowWidget::itemSize(QToolButton* btn) const
{
    return btn->sizeHint().boundedTo(btn->maximumSize()).expandedTo(btn->minimumSize());
}

int TaskBarFlowWidget::spacing() const
{
    return toolBar->style()->pixelMetric(QStyle::PM_ToolBarItemSpacing, nullptr, toolBar);
}

QSize TaskBarFlowWidget::calculate(int available, QList<QRect>* geometries) const
{
    bool horizontal = (flow == Qt::Horizontal);
    int space = spacing();

    // Split buttons into lines
    QList<QList<int>> lines;
    QList<int> lineThickness;
    QList<int> currentLine;
    int pos = 0;
    int thickness = 0;
    for (int i = 0; i < buttons.size(); i++)
    {
        QSize s = itemSize(buttons[i]);
        int length = horizontal ? s.width() : s.height();
        if (!currentLine.isEmpty() && pos + length > available)
        {
            lines << currentLine;
            lineThickness << thickness;
            currentLine.clear();
            pos = 0;
            thickness = 0;
        }
        currentLine << i;
        pos += length + space;
        thickness = qMax(thickness, horizontal ? s.height() : s.width());
    }
    if (!currentLine.isEmpty())
    {
        lines << currentLine;
        lineThickness << thickness;
    }

    // Place buttons. Each button gets full thickness of its line, so lines look even.
    if (geometries)
    {
        geometries->clear();
        for (int i = 0; i < buttons.size(); i++)
            *geometries << QRect();
    }

    int perpPos = 0;
    int maxAlong = 0;
    for (int lineIdx = 0; lineIdx < lines.size(); lineIdx++)
    {
        int along = 0;
        int lineTh = lineThickness[lineIdx];
        for (int i : lines[lineIdx])
        {
            QSize s = itemSize(buttons[i]);
            int length = horizontal ? s.width() : s.height();
            if (geometries)
                (*geometries)[i] = horizontal ? QRect(along, perpPos, length, lineTh) : QRect(perpPos, along, lineTh, length);

            along += length + space;
        }
        maxAlong = qMax(maxAlong, along - space);
        perpPos += lineTh + space;
    }

    int perp = lines.isEmpty() ? 0 : (perpPos - space);
    return QSize(maxAlong, perp);
}

void TaskBarFlowWidget::relayout()
{
    // Until the toolbar assigns the geometry, the widget has a default size, which has nothing to do with the space
    // available in the toolbar. Laying out buttons for that size would request a lot of lines.
    int available = (flow == Qt::Horizontal) ? width() : height();
    if (!testAttribute(Qt::WA_Resized) || available <= 0)
        available = INT_MAX;

    QList<QRect> geometries;
    QSize occupied = calculate(available, &geometries);
    for (int i = 0; i < buttons.size(); i++)
        buttons[i]->setGeometry(geometries[i]);

    perpendicularExtent = occupied.height();
}

TaskBar::TaskBar(const QString& title, QWidget *parent) :
    QToolBar(title, parent), taskGroup(this)
{
    init();
}

TaskBar::TaskBar(QWidget* parent) :
    QToolBar(parent), taskGroup(this)
{
    init();
}

QAction* TaskBar::addTask(const QIcon& icon, const QString& text)
{
    QAction* action = nullptr;
    if (wrapping)
    {
        action = new QAction(icon, text, this);
        flowWidget->insertButton(-1, createWrappedButton(action));
    }
    else
        action = QToolBar::addAction(icon, text);

    tasks << action;
    QToolButton* btn = getToolButton(action);
    if (!btn)
        return action;

    setupTaskButton(btn);
    taskGroup.addAction(action);
    return action;
}

void TaskBar::removeTask(QAction* action)
{
    tasks.removeOne(action);
    taskGroup.removeAction(action);
    if (wrapping)
    {
        QToolButton* btn = wrappedButtons.take(action);
        if (btn)
        {
            flowWidget->removeButton(btn);
            btn->hide();
            btn->deleteLater(); // might be called from within the button's event (i.e. middle-click closing)
        }
    }
    else
        removeAction(action);
}

bool TaskBar::isWrapping() const
{
    return wrapping;
}

void TaskBar::setWrapping(bool enabled)
{
    if (enabled == wrapping)
        return;

    if (enabled)
    {
        if (!flowWidget)
        {
            flowWidget = new TaskBarFlowWidget(this);
            flowWidgetAction = addWidget(flowWidget);
            connect(this, &QToolBar::orientationChanged, flowWidget, [this](Qt::Orientation o)
            {
                flowWidget->setFlow(o);
            });
        }

        for (QAction* task : tasks)
            removeAction(task);

        wrapping = true;
        for (QAction* task : tasks)
        {
            QToolButton* btn = createWrappedButton(task);
            flowWidget->insertButton(-1, btn);
            setupTaskButton(btn);
        }
        flowWidget->setFlow(orientation());
        flowWidgetAction->setVisible(true);
    }
    else
    {
        flowWidgetAction->setVisible(false);
        for (QAction* task : tasks)
        {
            QToolButton* btn = wrappedButtons.take(task);
            if (!btn)
                continue;

            flowWidget->removeButton(btn);
            btn->hide();
            btn->deleteLater();
        }

        wrapping = false;
        for (QAction* task : tasks)
        {
            addAction(task);
            QToolButton* btn = getToolButton(task);
            if (btn)
                setupTaskButton(btn);
        }
    }
}

QToolButton* TaskBar::createWrappedButton(QAction* task)
{
    // Mimics buttons created by QToolBar for its actions.
    QToolButton* btn = new QToolButton(flowWidget);
    btn->setAutoRaise(true);
    btn->setFocusPolicy(Qt::NoFocus);
    btn->setIconSize(iconSize());
    btn->setToolButtonStyle(toolButtonStyle());
    btn->setDefaultAction(task);
    connect(this, &QToolBar::iconSizeChanged, btn, &QToolButton::setIconSize);
    connect(this, &QToolBar::toolButtonStyleChanged, btn, &QToolButton::setToolButtonStyle);
    wrappedButtons[task] = btn;
    return btn;
}

void TaskBar::setupTaskButton(QToolButton* btn)
{
    // A workaround for QAction button (or QToolBar itself) that takes over (and doesn't propagate) mousePressEvent.
    btn->setMaximumWidth(400);
    btn->installEventFilter(this);
    connect(btn, SIGNAL(pressed()), this, SLOT(mousePressed()), Qt::UniqueConnection);
}

QRect TaskBar::getTaskRect(QAction* task)
{
    QToolButton* btn = getToolButton(task);
    if (!btn)
        return QRect();

    return QRect(btn->mapTo(this, QPoint(0, 0)), btn->size());
}

QAction* TaskBar::getTaskAt(const QPoint& position)
{
    if (!wrapping)
    {
        QAction* action = actionAt(position);
        return tasks.contains(action) ? action : nullptr;
    }

    for (QAction* task : tasks)
    {
        if (getTaskRect(task).contains(position))
            return task;
    }
    return nullptr;
}

QList<QAction*> TaskBar::getTasks() const
{
    return tasks;
}

void TaskBar::init()
{
    setAcceptDrops(true);
    setStyleSheet(R"(
        QToolButton:checked {
            color: white;
            background-color: #0078D7;
            border: 2px solid #005A9E;
            border-radius: 4px;
        }
        QToolButton:hover {
            background-color: #005A9E;
            color: white;
        }
    )");
}

void TaskBar::mousePressed()
{
    dragStartPosition = mapFromGlobal(QCursor::pos());
    dragStartTask = getTaskAt(dragStartPosition);
    if (dragStartTask)
        dragStartTask->trigger();
}

int TaskBar::getActiveTaskIdx()
{
    QAction* checked = taskGroup.checkedAction();
    if (!checked)
    {
        // Looks like no tasks yet.
        return -1;
    }

    return tasks.indexOf(checked);
}

void TaskBar::nextTask()
{
    int idx = getActiveTaskIdx() + 1;
    if (tasks.size() <= idx)
        return;

    tasks[idx]->trigger();
}

void TaskBar::prevTask()
{
    int idx = getActiveTaskIdx() - 1;
    if (idx < 0)
        return;

    tasks[idx]->trigger();
}

void TaskBar::moveTaskLater()
{
    int idx = getActiveTaskIdx();
    if (idx < 0)
        return;

    QAction* task = tasks[idx];
    idx++;
    idx++; // moving right actually needs idx+2 because of the way that dragTaskTo() works.
    dragTaskTo(task, idx);
}

void TaskBar::moveTaskEarlier()
{
    int idx = getActiveTaskIdx();
    if (idx < 0)
        return;

    QAction* task = tasks[idx];
    idx--;
    dragTaskTo(task, idx);
}

void TaskBar::setActiveTask(QAction* task)
{
    if (!task)
        return;

    task->trigger();
}

void TaskBar::initContextMenu(ExtActionContainer* mainWin)
{
    // MainWindow is passed as argument to this function, so it's not referenced with MAINWINDOW macro,
    // because that macro causes MainWindow initialization and this caused endless loop.
    taskMenu = new QMenu(this);
    taskMenu->addAction(mainWin->getAction(MainWindow::CLOSE_WINDOW));
    taskMenu->addAction(mainWin->getAction(MainWindow::CLOSE_ALL_WINDOWS));
    taskMenu->addAction(mainWin->getAction(MainWindow::CLOSE_OTHER_WINDOWS));
    taskMenu->addAction(mainWin->getAction(MainWindow::CLOSE_ALL_WINDOWS_LEFT));
    taskMenu->addAction(mainWin->getAction(MainWindow::CLOSE_ALL_WINDOWS_RIGHT));
    taskMenu->addSeparator();
    taskMenu->addAction(mainWin->getAction(MainWindow::PREV_TASK));
    taskMenu->addAction(mainWin->getAction(MainWindow::NEXT_TASK));
    taskMenu->addAction(mainWin->getAction(MainWindow::MOVE_TASK_EARLIER));
    taskMenu->addAction(mainWin->getAction(MainWindow::MOVE_TASK_LATER));
    taskMenu->addSeparator();
    taskMenu->addAction(mainWin->getAction(MainWindow::RESTORE_WINDOW));
    taskMenu->addAction(mainWin->getAction(MainWindow::RENAME_WINDOW));
    taskMenu->addSeparator();

    wrapAction = new QAction(tr("Wrap window list into multiple rows"), this);
    wrapAction->setCheckable(true);
    connect(wrapAction, &QAction::toggled, this, [](bool checked)
    {
        CFG_UI.General.WrapWindowList.set(checked);
    });
    taskMenu->addAction(wrapAction);

    CONFLICTING_MENU_HOTKEY_WORKAROUND(taskMenu, QKeySequence::Close, MAINWINDOW->getAction(MainWindow::Action::CLOSE_WINDOW));
    connect(this, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(taskBarMenuRequested(QPoint)));
    connect(CFG_UI.General.WrapWindowList, SIGNAL(changed(QVariant)), this, SLOT(applyWrapConfig()));
    applyWrapConfig();
}

void TaskBar::applyWrapConfig()
{
    bool enabled = CFG_UI.General.WrapWindowList.get();
    if (wrapAction)
    {
        QSignalBlocker blocker(wrapAction);
        wrapAction->setChecked(enabled);
    }
    setWrapping(enabled);
}

void TaskBar::taskBarMenuRequested(const QPoint &p)
{

    QAction* task = getTaskAt(p);
    bool taskClicked = (task != nullptr);
    if (taskClicked)
        task->trigger();

    MAINWINDOW->updateWindowActions();
    taskMenu->popup(mapToGlobal(p));
}

QToolButton* TaskBar::getToolButton(QAction* action)
{
    if (wrapping)
        return wrappedButtons.value(action);

    return dynamic_cast<QToolButton*>(widgetForAction(action));
}

QAction* TaskBar::getNextClosestAction(const QPoint& position)
{
    if (wrapping)
    {
        // Tasks are laid out in multiple lines (rows, or columns for vertical orientation).
        // First find the line closest to the position, then the first task in that line that begins after the position.
        // If there is none, the first task of the next line is the closest one.
        bool horizontal = (orientation() == Qt::Horizontal);
        int perpPos = horizontal ? position.y() : position.x();
        int alongPos = horizontal ? position.x() : position.y();
        int lineStart = -1;
        int bestDistance = INT_MAX;
        for (QAction* task : tasks)
        {
            QRect r = getTaskRect(task);
            if (r.isNull())
                continue;

            int start = horizontal ? r.top() : r.left();
            int end = horizontal ? r.bottom() : r.right();
            int distance = (perpPos < start) ? (start - perpPos) : ((perpPos > end) ? (perpPos - end) : 0);
            if (distance < bestDistance)
            {
                bestDistance = distance;
                lineStart = start;
            }
        }

        for (QAction* task : tasks)
        {
            QRect r = getTaskRect(task);
            if (r.isNull())
                continue;

            int start = horizontal ? r.top() : r.left();
            if (start > lineStart)
                return task;

            if (start == lineStart && (horizontal ? r.x() : r.y()) >= alongPos)
                return task;
        }
        return nullptr;
    }

    if (orientation() == Qt::Horizontal)
    {
        for (QAction*& action : tasks)
        {
            QToolButton* btn = getToolButton(action);
            if (btn && btn->x() >= position.x())
                return action;
        }
    }
    else
    {
        for (QAction* action : tasks)
        {
            QToolButton* btn = getToolButton(action);
            if (btn && btn->y() >= position.y())
                return action;
        }
    }
    return nullptr;
}

void TaskBar::mousePressEvent(QMouseEvent* event)
{
    QToolBar::mousePressEvent(event);
    dragStartTask = nullptr;
}

void TaskBar::mouseMoveEvent(QMouseEvent *event)
{
    if (!handleMouseMoveEvent(event))
        QToolBar::mouseMoveEvent(event);
}

bool TaskBar::handleMouseMoveEvent(QMouseEvent* event)
{
    if (!(event->buttons() & Qt::LeftButton))
        return false;

    if (!dragStartTask)
        return false;

    if ((event->pos() - dragStartPosition).manhattanLength() < QApplication::startDragDistance())
        return false;

    QDrag *drag = new QDrag(this);
    drag->setMimeData(generateMimeData());

    dragStartIndex = tasks.indexOf(dragStartTask);
    drag->exec(Qt::MoveAction);
    return true;
}

void TaskBar::dragEnterEvent(QDragEnterEvent *event)
{
    if (!hasTaskBarItem(event->mimeData()))
        return;

    dragTaskTo(dragStartTask, event->position().toPoint());
    event->acceptProposedAction();
}

void TaskBar::dragMoveEvent(QDragMoveEvent* event)
{
    if (!hasTaskBarItem(event->mimeData()))
        return;

    dragTaskTo(dragStartTask, event->position().toPoint());
    event->acceptProposedAction();
}

void TaskBar::dropEvent(QDropEvent *event)
{
    event->acceptProposedAction();
}

bool TaskBar::eventFilter(QObject* obj, QEvent* event)
{
    if (event->type() == QEvent::MouseButtonPress && dynamic_cast<QMouseEvent*>(event)->button() == Qt::MiddleButton)
    {
        QToolButton* btn = dynamic_cast<QToolButton*>(obj);
        if (btn && btn->defaultAction())
        {
            btn->defaultAction()->trigger();
            MDIAREA->closeActiveSubWindow();
            return true;
        }
    }
    return QObject::eventFilter(obj, event);
}

void TaskBar::dragTaskTo(QAction* task, const QPoint& position)
{
    int idx = getDropPositionIndex(task, position);
    if (idx < 0)
        return;

    dragTaskTo(task, idx);
}

void TaskBar::dragTaskTo(QAction* task, int positionIndex)
{
    if (positionIndex < 0)
        return;

    dragCurrentIndex = positionIndex;

    int oldIdx = tasks.indexOf(task);
    if (oldIdx < 0)
        return;

    // The positionIndex points to the task that the moved task should be placed just before.
    // If we move from left to right, it actually points to 1 position after,
    // so to get the precise position index on the list, we deduct 1 from the index.
    int newTaskIdx;
    if (positionIndex >= tasks.size())
        newTaskIdx = tasks.size() - 1;
    else
        newTaskIdx = (positionIndex > oldIdx) ? (positionIndex - 1) : positionIndex;

    if (oldIdx == newTaskIdx)
        return;

    tasks.move(oldIdx, newTaskIdx);
    updateTaskPosition(task);
}

void TaskBar::updateTaskPosition(QAction* task)
{
    int idx = tasks.indexOf(task);
    if (wrapping)
    {
        flowWidget->moveButton(wrappedButtons.value(task), idx);
        return;
    }

    removeAction(task);
    if (idx + 1 < tasks.size())
        insertAction(tasks.at(idx + 1), task);
    else
        addAction(task);

    // Toolbar creates a new button for re-inserted action
    QToolButton* btn = getToolButton(task);
    if (btn)
        setupTaskButton(btn);
}

QMimeData* TaskBar::generateMimeData()
{
    QMimeData *mimeData = new QMimeData();
    mimeData->setData(mimeDataId, QByteArray());
    return mimeData;
}

int TaskBar::getDropPositionIndex(QAction* task, const QPoint& position)
{
    QAction* action = getTaskAt(position);
    bool overTask = (action != nullptr);
    if (!action)
        action = getNextClosestAction(position);

    if (!action)
        return tasks.size(); // We moved completly out of actions range, report last possible position.

    if (action == task)
        return -1;

    int newIdx = tasks.indexOf(action);

    // With multiple lines the closest task may be in the next line, so its coordinates cannot be compared with position.
    // Not being over any task, we just place the dragged task before the closest one.
    if (wrapping && !overTask)
        return newIdx;

    QRect btnRect = getTaskRect(action);
    int actionBeginPos;
    int actionEndPos;
    int newPos;
    if (orientation() == Qt::Horizontal)
    {
        actionBeginPos = btnRect.x();
        actionEndPos = btnRect.x() + btnRect.width();
        newPos = position.x();
    }
    else
    {
        actionBeginPos = btnRect.y();
        actionEndPos = btnRect.y() + btnRect.height();
        newPos = position.y();
    }

    if (dragCurrentIndex <= newIdx)
    {
        // D&D from left to right
        if (newPos >= actionBeginPos)
            return newIdx + 1;
        else
            return newIdx;

    }
    else
    {
        // D&D from right to left
        if (newPos <= actionEndPos)
            return newIdx;
        else
            return newIdx + 1;
    }

    return -1; // This also should never happen. All cases should be covered above. But just in case.
}

bool TaskBar::isEmpty()
{
    return tasks.isEmpty();
}

int TaskBar::count()
{
    return tasks.count();
}

QAction* TaskBar::getActiveTask() const
{
    QAction* checked = taskGroup.checkedAction();
    if (!checked)
        return nullptr;

    return checked;
}

QAction* TaskBar::getNextTask(QAction* from) const
{
    if (!from)
        from = getActiveTask();

    if (!from)
        return nullptr;

    int idx = tasks.indexOf(from);
    idx++;
    if (idx < tasks.size())
        return tasks[idx];

    return nullptr;
}

QAction* TaskBar::getPrevTask(QAction* from) const
{
    if (!from)
        from = getActiveTask();

    if (!from)
        return nullptr;

    int idx = tasks.indexOf(from);
    idx--;
    if (idx > 0)
        return tasks[idx];

    return nullptr;
}

bool TaskBar::hasTaskBarItem(const QMimeData* data)
{
    return data && data->hasFormat(mimeDataId);
}
