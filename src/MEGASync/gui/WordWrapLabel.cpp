#include "WordWrapLabel.h"

#include <QDebug>
#include <QEvent>
#include <QApplication>
#include <QResizeEvent>
#include <QAbstractTextDocumentLayout>
#include <QTextBlock>
#include <QTextLayout>

#include <Utilities.h>

/**
 * THIS COMPONENT IS ONLY VALID WHEN THE LABEL HAS EXPANDING SIZE POLICY
 */

//This event is propagated from child to parent, this is why it is used
const QEvent::Type WordWrapLabel::HeightAdapted = QEvent::WhatsThisClicked;

WordWrapLabel::WordWrapLabel(QWidget* parent)
    : QTextBrowser(parent)
    , mLinkActivated(false)
    , mMaxHeight(-1)
    , mMaxLines(-1)
    , mFormat(Qt::PlainText)
{
    setFrameStyle(QFrame::NoFrame);
    setTextInteractionFlags(Qt::LinksAccessibleByMouse);
    setCursor(parent->cursor());
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    setOpenLinks(false);

    parent->installEventFilter(this);
    viewport()->installEventFilter(this);

    document()->setDocumentMargin(0);

    connect(this, &WordWrapLabel::anchorClicked, this, &WordWrapLabel::onLinkActivated);

    //Timer to avoid multiple height adaptations if you change the limit type
    //The height adaptation will be done in the following event loop
    mAdaptHeightTimer.setSingleShot(true);
    mAdaptHeightTimer.setInterval(0);
    connect(&mAdaptHeightTimer, &QTimer::timeout, this, &WordWrapLabel::onAdaptHeight);
}

void WordWrapLabel::setMaximumLines(int8_t lines)
{
    //Don´t use two limits at the same time
    Q_ASSERT_X(mMaxHeight == -1, "WordWrapLabel", "Use resetSizeLimits before using this method");
    mMaxLines = lines;
    mAdaptHeightTimer.start();
}

void WordWrapLabel::setMaximumHeight(int maxHeight)
{
    //Don´t use two limits at the same time
    Q_ASSERT_X(mMaxLines == -1, "WordWrapLabel", "Use resetSizeLimits before using this method");
    mMaxHeight = maxHeight;
    mAdaptHeightTimer.start();
}

void WordWrapLabel::resetSizeLimits()
{
    mMaxHeight = -1;
    mMaxLines = -1;
    QTextEdit::setText(mText);
    mAdaptHeightTimer.start();
}

void WordWrapLabel::setText(const QString& text)
{
    mText = text;

    if (mFormat == Qt::PlainText)
    {
        QTextEdit::setPlainText(mText);
    }
    else
    {
        QTextEdit::setText(mText);
    }
}

void WordWrapLabel::setTextFormat(Qt::TextFormat format)
{
    mFormat = format;
}

void WordWrapLabel::onAdaptHeight()
{
    QTextLayout* textLayout = document()->firstBlock().layout();
    if (!textLayout || mText.isEmpty())
    {
        return;
    }

    textLayout->beginLayout();

    int textHeight(0);
    int processedStringLength(0);
    int lineCounter(0);

    //This while is break when a new line is invalid
    //or we need to elide the last line
    while (true)
    {
        auto line = textLayout->createLine();
        if (!line.isValid())
        {
            break;
        }
        line.setLineWidth(width());
        auto lineHeight = line.height();
        textHeight += static_cast<int>(lineHeight);
        lineCounter++;
        processedStringLength += line.textLength();

        //Check if there is text in the last line
        if ((mMaxHeight > 0 && (textHeight + lineHeight) >= mMaxHeight) ||
            (mMaxLines > 0 && (lineCounter + 1 == mMaxLines)))
        {
            auto modifiedText(mText);
            auto textNotToElide(modifiedText.left(processedStringLength));
            auto textToElide(modifiedText.remove(0, processedStringLength));
            if (!textToElide.isEmpty())
            {
                auto elidedText(fontMetrics().elidedText(textToElide, Qt::ElideMiddle, width()));

                //Elide line height
                auto elideLine = textLayout->createLine();
                elideLine.setLineWidth(width());
                textHeight += static_cast<int>(elideLine.height());
                elidedText.prepend(textNotToElide);

                //QTextEdit::setText removes the textLayout so its important to not remove these lines
                textLayout->endLayout();
                textLayout = nullptr;

                QTextEdit::setText(elidedText);
                setToolTip(mText);

                break;
            }
        }
    }

    if (textLayout)
    {
        textLayout->endLayout();
    }

    setLineWrapColumnOrWidth(lineWrapColumnOrWidth());

    QSize docSize = document()->size().toSize();
    if (textHeight != 0 && docSize.height() != height())
    {
        setFixedHeight(textHeight);
        qApp->postEvent(this, new QEvent(HeightAdapted));
    }
}

void WordWrapLabel::resizeEvent(QResizeEvent* event)
{
    /*
      * If the widget has been resized then the size hint will
      * also have changed.  Call updateGeometry to make sure
      * any layouts are notified of the change.
      */
    updateGeometry();

    if (!mText.isEmpty())
    {
        //Here we dont use the timer as we want to do it right now, and not in the following event loop
        onAdaptHeight();
    }

    QTextEdit::resizeEvent(event);
}

bool WordWrapLabel::eventFilter(QObject* obj, QEvent* event)
{
    if(event->type() == QEvent::CursorChange)
    {
        if(viewport()->cursor().shape() != parentWidget()->cursor().shape())
        {
            setCursor(parentWidget()->cursor());
        }
    }
    return QTextEdit::eventFilter(obj, event);
}

void WordWrapLabel::mouseReleaseEvent(QMouseEvent* ev)
{
    QTextBrowser::mousePressEvent(ev);

    //In order to propagate the mouse release event
    //Otherwise, the QTextBrowser hijacks it even if you don´t press of the link
    if(!mLinkActivated)
    {
        QWidget::mousePressEvent(ev);
    }

    //Reset for next click
    mLinkActivated = false;
}

void WordWrapLabel::onLinkActivated(const QUrl& link)
{
    mLinkActivated = true;
    Utilities::openUrl(link);
}

void WordWrapLabel::setCursor(const QCursor& cursor)
{
    QTextEdit::setCursor(cursor);
    viewport()->setCursor(cursor);
}
