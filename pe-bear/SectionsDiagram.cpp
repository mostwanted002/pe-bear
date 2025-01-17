#include "SectionsDiagram.h"
#include "REbear.h"
#include "ViewSettings.h"

#include <math.h>

#ifndef PAGE_SIZE
#define PAGE_SIZE 0x1000
#endif

int getHPad()
{
	const int iconDim = ViewSettings::getSmallIconDim(QApplication::font());
	return iconDim;
}

int getMinLeftPad()
{
	const int iconDim = ViewSettings::getSmallIconDim(QApplication::font());
	return static_cast<int>(iconDim * 1.5);
}

int getMaxLeftPad()
{
	const int iconDim = ViewSettings::getSmallIconDim(QApplication::font());
	return static_cast<int>(iconDim * 5);
}

int getMinRightPad()
{
	const int iconDim = ViewSettings::getSmallIconDim(QApplication::font());
	return static_cast<int>(iconDim * 1.5);
}

int getMaxRightPad()
{
	const int iconDim = ViewSettings::getSmallIconDim(QApplication::font());
	return static_cast<int>(iconDim * 4);
}

int getMinBarWidth()
{
	const int iconDim = ViewSettings::getSmallIconDim(QApplication::font());
	return static_cast<int>(iconDim * 1.8);
}

//-----------------------------------------------------------

SecDiagramModel::SecDiagramModel(PeHandler* peHndl) 
	: QObject(), PeViewItem(peHndl), selectedStart(0), selectedEnd(0)
{
	if (myPeHndl) {
		this->selectedStart = myPeHndl->pageStart;
		this->selectedEnd = myPeHndl->pageStart + myPeHndl->pageSize;
		connect(myPeHndl, SIGNAL(secHeadersModified()), this, SLOT(onSectionsUpdated()));
	}
}

SectionHdrWrapper* SecDiagramModel::getSectionAtUnit(int unitNum, bool isRaw)
{
	const size_t totalUnits = getUnitsNum(isRaw);
	if (unitNum > totalUnits) return NULL;

	bufsize_t unitSize = this->getUnitSize(isRaw);
	const offset_t addr = unitSize * unitNum;
	Executable::addr_type aType = isRaw ? Executable::RAW : Executable::RVA;
	return this->m_PE->getSecHdrAtOffset(addr, aType, false);
}

int SecDiagramModel::secIndexAtUnit(int unitNum, bool isRaw)
{
	SectionHdrWrapper* sec = getSectionAtUnit(unitNum, isRaw);
	if (!sec) {
		return SectHdrsWrapper::SECT_INVALID_INDEX;
	}
	return m_PE->getSecIndex(sec);
}

void SecDiagramModel::setSelectedArea(offset_t selStart, bufsize_t pageSize)
{
	if (!this->myPeHndl) return;

	this->selectedStart = selStart;
	this->selectedEnd = selStart + pageSize;

	emit modelUpdated();
}

void SecDiagramModel::selectFromAddress(uint32_t offset)
{
	if (!this->myPeHndl) return;
	if (offset < 0 || offset >= m_PE->getRawSize()) return;
	
	this->selectedStart = offset;
	this->myPeHndl->setPageOffset(this->selectedStart);
	emit modelUpdated();
}

bufsize_t SecDiagramModel::getUnitSize(bool isRaw)
{
	const Executable::addr_type aType = isRaw ? Executable::RAW : Executable::RVA;
	const bufsize_t unit = m_PE ? m_PE->getAlignment(aType) : 0;
	return (unit > 0) ? unit : PAGE_SIZE;
}

size_t SecDiagramModel::getUnitsNum(bool isRaw)
{
	if (this->m_PE == NULL) {
		return 0;
	}
	const bufsize_t unitSize = this->getUnitSize(isRaw);
	const bufsize_t size = isRaw ? this->m_PE->getRawSize() : this->m_PE->getImageSize();
	if (!unitSize) return 0;

	return pe_util::unitsCount(size, unitSize);
}

size_t SecDiagramModel::unitsOfSection(int index, bool isRaw)
{
	if (this->m_PE == NULL) return 0;
	if (index > this->m_PE->getSectionsCount()) return 0;
	SectionHdrWrapper *sec = this->m_PE->getSecHdr(index);
	if (sec == NULL) return 0;

	const Executable::addr_type aType = isRaw ? Executable::RAW : Executable::RVA;
	const bufsize_t unitSize = this->getUnitSize(isRaw);
	const bufsize_t size = sec->getContentSize(aType, true);
	if (!unitSize) return 0;

	return pe_util::unitsCount(size, unitSize);
}

double SecDiagramModel::percentFilledInSection(int index, bool isRaw)
{
	if (!this->m_PE) return 0;
	if (index > this->m_PE->getSectionsCount()) return 0;
	
	SectionHdrWrapper *sec = this->m_PE->getSecHdr(index);
	if (!sec) return 0;

	const Executable::addr_type aType = isRaw ? Executable::RAW : Executable::RVA;
	bufsize_t size = sec->getContentSize(aType, true);

	bufsize_t unitSize = this->getUnitSize(isRaw);
	size_t units = unitsOfSection(index, isRaw);
	bufsize_t roundedSize = units * unitSize;
	if (roundedSize == 0) return 0;
	
	double res = double(size)/ double(roundedSize);
	return res;
}

double SecDiagramModel::unitOfSectionBegin(int index, bool isRaw)
{
	if (!this->m_PE) return 0;
	if (index > this->m_PE->getSectionsCount()) return 0;

	SectionHdrWrapper *sec = this->m_PE->getSecHdr(index);
	if (!sec) return 0;

	const Executable::addr_type aType = isRaw ? Executable::RAW : Executable::RVA;
	DWORD bgn = sec->getContentOffset(aType);
	return unitOfAddress(bgn, isRaw);
}

DWORD SecDiagramModel::getSectionBegin(int index, bool isRaw)
{
	if (!this->m_PE) return 0;
	if (index > this->m_PE->getSectionsCount()) return 0;

	SectionHdrWrapper *sec = this->m_PE->getSecHdr(index);
	if (!sec) return 0;

	const Executable::addr_type aType = isRaw ? Executable::RAW : Executable::RVA;
	DWORD bgn = sec->getContentOffset(aType);
	return bgn;
}

double SecDiagramModel::unitOfAddress(uint32_t address, bool isRaw)
{
	if (!this->m_PE) return (-1);

	bufsize_t unitSize = this->getUnitSize(isRaw);
	if (!unitSize) return (-1);
	return (double)address/(double)unitSize;
}

double SecDiagramModel::unitOfEntryPoint(bool isRaw)
{
	if (!this->m_PE) return -1;
	DWORD ep = this->m_PE->getEntryPoint();
	if (isRaw) {
		try {
			ep = this->m_PE->rvaToRaw(ep);
		} catch (CustomException e) {
			return -1;
		}
	}
	return unitOfAddress(ep, isRaw);
}

double SecDiagramModel::unitOfHeadersEnd(bool isRaw)
{
	if (!this->m_PE) return -1;
	uint32_t secHdrsOffset = m_PE->secHdrsEndOffset();
	return unitOfAddress(secHdrsOffset, isRaw);
}

DWORD SecDiagramModel::getEntryPoint(bool isRaw)
{
	if (!this->m_PE) return 0;
	DWORD ep = this->m_PE->getEntryPoint();
	if (!isRaw) return ep;
	try {
		ep = this->m_PE->rvaToRaw(ep);
	} catch (CustomException e) {
		return -1;
	}
	return ep;
}

QString SecDiagramModel::nameOfSection(int index)
{
	if (!this->m_PE) return "";
	if (index > this->m_PE->getSectionsCount()) return "";
	SectionHdrWrapper *sec = this->m_PE->getSecHdr(index);
	if (!sec) return "";
	return "[" + sec->mappedName + "]";
}

size_t SecDiagramModel::getSecNum()
{
	if (!this->m_PE) return 0;
	return this->m_PE->getSectionsCount();
}
//-----------------------------------------------------------

void SectionsDiagram::createActions()
{
	this->enableDrawEPAction = new QAction("Entry Point", this);
	this->enableDrawEPAction->setCheckable(true);
	connect(this->enableDrawEPAction, SIGNAL(triggered(bool)), this, SLOT(setDrawEP(bool)));

	this->enableDrawSecHdrsAction = new QAction("Sections Headers end", this);
	this->enableDrawSecHdrsAction->setCheckable(true);
	connect(this->enableDrawSecHdrsAction, SIGNAL(triggered(bool)), this, SLOT(setDrawSecHdrs(bool)));

	this->enableGridAction = new QAction("Grid", this);
	this->enableGridAction->setCheckable(true);
	connect(this->enableGridAction, SIGNAL(triggered(bool)), this, SLOT(setEnableGrid(bool)));

	this->enableOffsetsAction = new QAction("Sections &Offsets", this);
	this->enableOffsetsAction->setCheckable(true);
	connect(this->enableOffsetsAction, SIGNAL(triggered(bool)), this, SLOT(setDrawOffsets(bool)));

	this->enableSecNamesAction = new QAction("Sections &Names", this);
	this->enableSecNamesAction->setCheckable(true);
	connect(this->enableSecNamesAction, SIGNAL(triggered(bool)), this, SLOT(setDrawSecNames(bool)));
}

void SectionsDiagram::destroyActions()
{
	delete this->enableGridAction;
	delete this->enableDrawEPAction;
	delete this->enableDrawSecHdrsAction;
	delete this->enableOffsetsAction;
	delete this->enableSecNamesAction;
}

SectionsDiagram::SectionsDiagram(SecDiagramModel *model, bool viewRawAddresses , QWidget *parent)
	: QMainWindow(parent), myModel(NULL), isRaw(viewRawAddresses), menu(this),
	isDrawSecNames(true), isDrawOffsets(true), isDrawSelected(false),
	selY1(0), selY2(0),  needRefresh(true)
{
	setModel(model);
	this->setMouseTracking(true);

	this->contourColor = Qt::black;

	this->setContextMenuPolicy(Qt::CustomContextMenu);
	createActions();
	menu.addAction(this->enableGridAction);
	menu.addAction(this->enableDrawEPAction);
	menu.addAction(this->enableDrawSecHdrsAction);
	menu.addAction(this->enableOffsetsAction);
	menu.addAction(this->enableSecNamesAction);

	isGridEnabled = false;
	isDrawEPEnabled = true;
	isDrawSecHdrsEnabled = true;
	this->enableGridAction->setChecked(isGridEnabled);
	this->enableDrawEPAction->setChecked(isDrawEPEnabled);
	this->enableDrawSecHdrsAction->setChecked(isDrawSecHdrsEnabled);
	
	QColor bgColor(DIAGRAM_BG);
	bgColor.setAlpha(140);
	this->setBackgroundColor(bgColor);

	setAutoFillBackground(true);
	setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	setFocusPolicy(Qt::StrongFocus);

	connect( this, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(showMenu(QPoint)) );
}

void SectionsDiagram::mouseMoveEvent(QMouseEvent *event)
{
	int currY = event->pos().y();
	int unitNum = unitAtPosY(currY);
	if (unitNum == (-1)) return;

	int unitSize = this->myModel->getUnitSize(this->isRaw);
	if (unitSize <= 0) return;
	int value = unitSize * unitNum;
	SectionHdrWrapper *sec = this->myModel->getSectionAtUnit(unitNum, this->isRaw);
	QString secName = (sec) ? "\n[" + sec->mappedName + "]" : "";

	this->setToolTip(QString::number(value, 16) + secName);
}

void SectionsDiagram::setBackgroundColor(QColor bgColor)
{
	this->bgColor = bgColor;
	QPalette p = this->palette();
	p.setColor(QPalette::Base, bgColor);
	setPalette(p);
	setBackgroundRole(p.Base);
}

QSize SectionsDiagram::minimumSizeHint() const
{
	const int MIN_BAR_WIDTH = getMinBarWidth();
	
	const int MIN_LEFT_PAD = getMinLeftPad();
	const int MIN_RIGHT_PAD = getMinRightPad();
	
	const int MAX_LEFT_PAD = getMaxLeftPad();
	const int MAX_RIGHT_PAD = getMaxRightPad();

	const int Y_TOP = getHPad();
	
	if (!this->isDrawOffsets && !this->isDrawSecNames)
		return QSize((MIN_LEFT_PAD + MIN_BAR_WIDTH + MIN_RIGHT_PAD), Y_TOP);

	return QSize((MAX_LEFT_PAD + MIN_BAR_WIDTH + MAX_RIGHT_PAD), 4 * Y_TOP);
}

QSize SectionsDiagram::sizeHint() const
{
	//const int Y_TOP = getMaxRightPad() * 5;
	
	QSize size = minimumSizeHint();
	return QSize(size.width() * 5,  size.height() * 5);
}

void SectionsDiagram::paintEvent(QPaintEvent *)
{
	QStylePainter painter(this);
	painter.drawPixmap(0, 0, this->pixmap);
	if (hasFocus()) {
		QStyleOptionFocusRect option;
		option.initFrom(this);
		option.backgroundColor = palette().dark().color();
		painter.drawPrimitive(QStyle::PE_FrameFocusRect, option);
	}
}

void SectionsDiagram::setModel(SecDiagramModel *model)
{
	if (this->myModel)
		disconnect(this->myModel, SIGNAL(modelUpdated()), this, SLOT(refreshPixmap()) );
	
	this->myModel = model;

	if (this->myModel)
		connect(this->myModel, SIGNAL(modelUpdated()), this, SLOT(refreshPixmap()) );
}

void SectionsDiagram::showMenu(QPoint p)
{ 
	this->enableDrawEPAction->setChecked(isDrawEPEnabled);
	this->enableDrawSecHdrsAction->setChecked(this->isDrawSecHdrsEnabled);
	this->enableGridAction->setChecked(this->isGridEnabled);

	this->enableOffsetsAction->setChecked(this->isDrawOffsets);
	this->enableSecNamesAction->setChecked(this->isDrawSecNames);
	menu.exec(mapToGlobal(p)); 
}

void SectionsDiagram::refreshPixmap()
{
	if (!this->isVisible()) {
		 needRefresh = true;
		return;
	}
	needRefresh = false;
	pixmap = QPixmap(size());
	pixmap.fill(this->bgColor);
	
	QPainter painter(&pixmap);
	if (this->myModel) 
		drawSections(&painter);
	update();
}

void SectionsDiagram::drawSections(QPainter *painter)
{
	const int MIN_LEFT_PAD = getMinLeftPad();
	const int MIN_RIGHT_PAD = getMinRightPad();
	
	const int MAX_LEFT_PAD = getMaxLeftPad();
	const int MAX_RIGHT_PAD = getMaxRightPad();
	
	const int MIN_BAR_WIDTH = getMinBarWidth();
	
	const int HPAD = getHPad();
	
	int LEFT_PAD = MAX_LEFT_PAD;
	int RIGHT_PAD = MAX_RIGHT_PAD;
	
	if (!this->isDrawSecNames && !this->isDrawOffsets) {
		LEFT_PAD = MIN_LEFT_PAD;
		RIGHT_PAD = MIN_RIGHT_PAD;
	}

	QRect rect(LEFT_PAD, HPAD, width() - (LEFT_PAD + RIGHT_PAD), height() - 2 * HPAD);
	if (!rect.isValid())
		return;

	QPen borderPen(Qt::lightGray);
	QPen descPen(Qt::red);
	QPen textPen(this->contourColor);

	size_t totalUnits = this->myModel->getUnitsNum(this->isRaw);
	if (totalUnits == 0) return;

	painter->setPen(textPen);
	painter->drawRect(rect.adjusted(0, 0, -1, -1));

	int textY = 0;
	int prevSecNum = 0;
	size_t secNum = this->myModel->getSecNum();

	/* draw grid */
	painter->setPen(borderPen);
	const int MAX_TO_DRAW = 1000;
	if (isGridEnabled ) {
		for (int j = 0; j < totalUnits && j < MAX_TO_DRAW; j++) {
			int y = rect.top() + (j * (rect.height() - 1) / totalUnits);
			painter->drawLine(rect.left(), y, rect.right(), y);
		}
	}

	if (secNum > 0) {
		size_t secUnits = 0;
		const size_t COLORS_NUM = 5;
			QColor colors[COLORS_NUM] = {
			QColor(0, 0, 255, 100),
			QColor(255, 255, 0, 100),
			QColor(25, 255, 0, 100),
			QColor(255, 34, 0, 100),
			QColor(200, 0, 255, 100)
		};
		/* draw sections */
		painter->setPen(textPen);
		int secIndex = -1;
		for (int j = 0; j < secNum; j++) {
			double startPosition = this->myModel->unitOfSectionBegin(j, isRaw);
			int yBgn = rect.top() + (startPosition * (rect.height() - 1) / totalUnits);
			if (isDrawOffsets) {
				DWORD secBgn = this->myModel->getSectionBegin(j, isRaw);
				QString name = QString::number(secBgn, 16).toUpper();
				painter->drawText(5, yBgn, name);
			}
			painter->drawLine(rect.left(), yBgn, rect.right(), yBgn);
			secUnits = this->myModel->unitsOfSection(j, isRaw);
			if (secUnits > 0) {
				int h = (secUnits * rect.height()) / totalUnits;
				h = h * this->myModel->percentFilledInSection(j, isRaw);
				h += (h > 0)? 1: 0; 
				int w = rect.right() - rect.left();
				painter->fillRect(QRect(rect.left() + 1, yBgn, w, h), colors[j % COLORS_NUM]);
			}
			if (isDrawSecNames) {
				QString secName = this->myModel->nameOfSection(j);
				painter->drawText(rect.left() + MIN_BAR_WIDTH, yBgn + HPAD, secName);
			}
		}
	}

	if (isDrawEPEnabled) drawEntryPoint(painter, rect, LEFT_PAD, RIGHT_PAD);
	if (isDrawSecHdrsEnabled) drawSecHeaders(painter, rect, LEFT_PAD, RIGHT_PAD);
	if (isDrawSelected) drawSelected(painter, rect, LEFT_PAD, RIGHT_PAD);
}

void SectionsDiagram::drawSecHeaders(QPainter *painter, QRect &rect, int LEFT_PAD, int RIGHT_PAD)
{
	const size_t totalUnits = this->myModel->getUnitsNum(this->isRaw);
	if (totalUnits == 0) return;
	
	double hdrsEndPosition = this->myModel->unitOfHeadersEnd(isRaw);
	if (hdrsEndPosition >= 0) {
		QPen hdrPen(this->contourColor);
		hdrPen.setStyle(Qt::DashLine);
		
		painter->setPen(hdrPen);
		int y = rect.top() + (hdrsEndPosition * (rect.height() - 1) / totalUnits);
		painter->drawLine(rect.left() - LEFT_PAD, y, rect.right() + RIGHT_PAD, y);
		painter->setPen(Qt::SolidLine);
	}
}

void SectionsDiagram::drawEntryPoint(QPainter *painter, QRect &rect, int LEFT_PAD, int RIGHT_PAD)
{
	const size_t totalUnits = this->myModel->getUnitsNum(this->isRaw);
	if (totalUnits == 0) return;
	
	QPen epPen(Qt::red);
	painter->setPen(epPen);
	
	double epUnitPosition = this->myModel->unitOfEntryPoint(isRaw);
	DWORD ep = this->myModel->getEntryPoint(isRaw);
	
	if (epUnitPosition >= 0) {
		painter->setPen(QColor(255, 0, 0));
		int y = rect.top() + (epUnitPosition * (rect.height() - 1) / totalUnits);
		painter->drawLine(rect.left() - LEFT_PAD, y, rect.right() + RIGHT_PAD, y);
	
		QString name = QString::number(ep, 16).toUpper();
		painter->drawText(5, y, name);
	}
}

int SectionsDiagram::unitAtPosY(int posY)
{
	const int MIN_LEFT_PAD = getMinLeftPad();
	const int MIN_RIGHT_PAD = getMinRightPad();
	
	const int MAX_LEFT_PAD = getMaxLeftPad();
	const int MAX_RIGHT_PAD = getMaxRightPad();
	
	const int HPAD = getHPad();

	int LEFT_PAD = MAX_LEFT_PAD;
	int RIGHT_PAD = MAX_RIGHT_PAD;
	
	if (!this->isDrawSecNames && !this->isDrawOffsets) {
		LEFT_PAD = MIN_LEFT_PAD;
		RIGHT_PAD = MIN_RIGHT_PAD;
	}

	QRect rect(LEFT_PAD, HPAD, width() - (LEFT_PAD + RIGHT_PAD), height() - 2 * HPAD);
	if (!rect.isValid())
		return (-1);

	size_t totalUnits = this->myModel->getUnitsNum(this->isRaw);
	if (totalUnits == 0) return (-1);
	
	int prevY = 0;
	/* grid */
	for (int unitNum = 0; unitNum < totalUnits; unitNum++) {
		int y = rect.top() + (unitNum * (rect.height() - 1) / totalUnits);
		if (posY > prevY && posY <= y) return unitNum;
		prevY = y;
	}
	return (-1);
}

//--------------------------------------------------------------------------------------

SelectableSecDiagram::SelectableSecDiagram(SecDiagramModel *model, bool isRawView, QWidget *parent)
	: SectionsDiagram(model, isRawView, parent)
{
	this->setMouseTracking(true);
	
	bgColor = QColor(DIAGRAM_BG);
	selectionColor = QColor(DIAGRAM_BG);
	selectionColor.setAlpha(150);
	cursorUpPix = QPixmap(":/icons/arr_up.ico");
	cursorDownPix = QPixmap(":/icons/arr_down.ico");
	upCursor = QCursor(cursorUpPix, 0, 0);
	downCursor = QCursor(cursorDownPix, 0,0);
}

void SelectableSecDiagram::drawSelected(QPainter *painter, QRect &rect, int LEFT_PAD, int RIGHT_PAD)
{
	const size_t totalUnits = this->myModel->getUnitsNum(this->isRaw);
	if (totalUnits == 0) return;
	
	if (this->myModel->selectedEnd == 0 && this->myModel->selectedStart == 0) {
		return;
	}
	
	QPen epPen(selectionColor);
	
	epPen.setWidth(3);
	painter->setPen(epPen);

	double unit1 =  this->myModel->unitOfAddress(this->myModel->selectedStart, isRaw);
	double unit2 =  this->myModel->unitOfAddress(this->myModel->selectedEnd, isRaw);

	double val = (unit1 /totalUnits) * (rect.height() - 1);
	int y1 = rect.top() + val;
		
	val = (unit2 /totalUnits) * (rect.height() - 1);
	int y2 = rect.top() + val + 1;

	QRect rect1( QPoint(rect.left() - LEFT_PAD, y1), QPoint(rect.right() + RIGHT_PAD, y2) );
	selY1 = y1;
	selY2 = y2;
	painter->fillRect(rect1, selectionColor);
}

void SelectableSecDiagram::mousePressEvent(QMouseEvent *event)
{
	if (event->button() != Qt::LeftButton) return;

	int currY = event->pos().y();
	int unitNum = unitAtPosY(currY);
	if (unitNum == (-1)) return;

	int unitSize = this->myModel->getUnitSize(true);
	if (unitSize <= 0) unitSize = PAGE_SIZE;
	int value = unitSize * unitNum;

	this->myModel->selectFromAddress(unitSize * unitNum);
}

void SelectableSecDiagram::mouseMoveEvent(QMouseEvent *event)
{
	int currY = event->pos().y();
	if (currY < selY1) {
		this->setCursor(upCursor);
	} else if (currY > selY2) {
		this->setCursor(downCursor);
	}
	SectionsDiagram::mouseMoveEvent(event);
}
