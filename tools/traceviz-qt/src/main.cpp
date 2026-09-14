#include <QApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMainWindow>
#include <QPainter>
#include <QSlider>
#include <QSpinBox>
#include <QTimer>
#include <QToolBar>
#include <QVBoxLayout>
#include <QWidget>

class TraceView final : public QWidget {
public:
    QJsonObject root;
    int frame = 0;
    int selectedAgent = 0;
    explicit TraceView(QWidget* parent = nullptr) : QWidget(parent) { setMinimumSize(640, 480); }
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this); p.fillRect(rect(), QColor("#101820"));
        const int rows = root.value("rows").toInt(), cols = root.value("cols").toInt();
        const QJsonArray frames = root.value("frames").toArray();
        if (rows <= 0 || cols <= 0 || frames.isEmpty()) return;
        const double cell = qMin(width() / double(cols), height() / double(rows));
        const QRectF board((width()-cols*cell)/2, (height()-rows*cell)/2, cols*cell, rows*cell);
        p.fillRect(board, QColor("#f8fafc"));
        const QJsonArray map = root.value("map").toArray();
        p.setPen(Qt::NoPen); p.setBrush(QColor("#475569"));
        for (int loc=0; loc<map.size(); ++loc) if (map.at(loc).toInt()) p.drawRect(QRectF(board.left()+(loc%cols)*cell, board.top()+(loc/cols)*cell, cell, cell));
        const QJsonObject current = frames.at(qBound(0, frame, frames.size()-1)).toObject();
        for (const auto& value : current.value("agents").toArray()) {
            const QJsonObject a = value.toObject(); const int loc=a.value("location").toInt(-1); if(loc<0) continue;
            const bool selected = a.value("id").toInt()==selectedAgent;
            p.setBrush(selected ? QColor("#dc2626") : QColor("#2563eb"));
            p.drawEllipse(QRectF(board.left()+(loc%cols)*cell+cell*.15, board.top()+(loc/cols)*cell+cell*.15, cell*.7, cell*.7));
            const int goal=a.value("goal").toInt(-1); if(goal>=0){ p.setBrush(Qt::NoBrush); p.setPen(QPen(QColor("#16a34a"), qMax(1.0, cell*.10))); p.drawRect(QRectF(board.left()+(goal%cols)*cell+cell*.12, board.top()+(goal/cols)*cell+cell*.12, cell*.76, cell*.76)); p.setPen(Qt::NoPen); }
        }
    }
};

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    const QStringList args=app.arguments(); const int traceArg=args.indexOf("--trace");
    if(traceArg<0 || traceArg+1>=args.size()) return 1;
    QFile file(args.at(traceArg+1)); if(!file.open(QIODevice::ReadOnly)) return 2;
    const QJsonObject root=QJsonDocument::fromJson(file.readAll()).object(); const QJsonArray frames=root.value("frames").toArray();
    if(root.value("format").toString()!="lifelongta-debug-trace-v1" || frames.isEmpty()) return 3;
    QMainWindow window; window.setWindowTitle("LifelongTA TraceViz Qt");
    auto* view=new TraceView; view->root=root; window.setCentralWidget(view);
    auto* toolbar=window.addToolBar("Replay"); auto* slider=new QSlider(Qt::Horizontal); slider->setRange(0,frames.size()-1); slider->setMinimumWidth(360);
    auto* agent=new QSpinBox; agent->setRange(0,qMax(0,root.value("teamSize").toInt()-1));
    auto* label=new QLabel; toolbar->addWidget(new QLabel("Step ")); toolbar->addWidget(slider); toolbar->addWidget(new QLabel(" Agent ")); toolbar->addWidget(agent); toolbar->addWidget(label);
    QObject::connect(slider,&QSlider::valueChanged,[&](int value){view->frame=value; const auto f=frames.at(value).toObject(); label->setText(QString("t=%1, changes=%2").arg(f.value("timestep").toInt()).arg(f.value("assignmentChanges").toArray().size())); view->update();});
    QObject::connect(agent,qOverload<int>(&QSpinBox::valueChanged),[&](int value){view->selectedAgent=value;view->update();});
    auto* timer=new QTimer(&window); timer->setInterval(120); QObject::connect(timer,&QTimer::timeout,[&](){slider->setValue(slider->value()>=slider->maximum()?0:slider->value()+1);});
    auto* play=toolbar->addAction("Play"); QObject::connect(play,&QAction::triggered,[&](){timer->isActive()?timer->stop():timer->start();play->setText(timer->isActive()?"Pause":"Play");});
    slider->setValue(0); window.resize(1100,800); window.show(); return app.exec();
}
