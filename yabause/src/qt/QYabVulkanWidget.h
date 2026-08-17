
#include <QWidget>


//class Renderer;
class YabauseThread;

class QYabVulkanWidget : public QWidget {

    Q_OBJECT

public:     
    explicit QYabVulkanWidget(QWidget *parent = nullptr);
    ~QYabVulkanWidget();
    static QYabVulkanWidget * getInstance() {
        return _instance;
    }

    //Renderer * getRenderer() {
    //    return _vulkanRenderer;
    //}

    void setYabauseThread(YabauseThread* p) {
      pYabauseThread = p;
      ready();  
    }

    void updateView(const QSize& size = QSize());

    void resizeEvent(QResizeEvent* event) override;

    // Null engine: Qt must not paint over the Vulkan surface.
    QPaintEngine* paintEngine() const override;

protected:
    void ready();
    static QYabVulkanWidget * _instance;    
    YabauseThread* pYabauseThread;
    //Renderer * _vulkanRenderer;
    void paintEvent(QPaintEvent* event) override;

};
