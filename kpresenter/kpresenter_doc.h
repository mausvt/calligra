/******************************************************************/
/* KPresenter - (c) by Reginald Stadlbauer 1998                   */
/* Version: 0.0.1                                                 */
/* Author: Reginald Stadlbauer                                    */
/* E-Mail: reggie@kde.org                                         */
/* needs c++ library Qt (http://www.troll.no)                     */
/* needs mico (http://diamant.vsb.cs.uni-frankfurt.de/~mico/)     */
/* needs OpenParts and Kom (weis@kde.org)                         */
/* written for KDE (http://www.kde.org)                           */
/* KPresenter is under GNU GPL                                    */
/******************************************************************/
/* Module: KPresenter Document (header)                           */
/******************************************************************/

#ifndef __kpresenter_doc_h__
#define __kpresenter_doc_h__


class KPresenterDocument_impl;
class KPresenterView_impl;

#include <document_impl.h>
#include <view_impl.h>

#include <qlist.h>
#include <qobject.h>
#include <qrect.h>
#include <qpainter.h>
#include <qcolor.h>
#include <qevent.h>
#include <qpixmap.h>
#include <qwmatrix.h>
#include <qbrush.h>
#include <qpen.h>
#include <qpopmenu.h>
#include <qcursor.h>

#include <koPageLayoutDia.h>

#include "kpresenter_view.h"
#include "global.h"
#include "qwmf.h"
#include "graphobj.h"
#include "ktextobject.h"

#define MIME_TYPE "application/x-kpresenter"
#define EDITOR "IDL:KPresenter/KPresenterDocument:1.0"

/******************************************************************/
/* class BackPic                                                  */
/******************************************************************/

class BackPic : public QWidget
{
  Q_OBJECT

public:

  // constructor - destructor
  BackPic(QWidget* parent=0,const char* name=0);
  ~BackPic();

  // get pictures
  void setClipart(const char*);
  const char* getClipart() {return (const char*)fileName;}
  QPicture* getPic();

private:

  // datas
  QPicture *pic;
  QString fileName;
  QWinMetaFile wmf;

};

/******************************************************************/
/* class KPresenterChild                                          */
/******************************************************************/
class KPresenterChild
{
public:

  // constructor - destructor
  KPresenterChild(KPresenterDocument_impl *_kpr,const QRect& _rect,OPParts::Document_ptr _doc);
  ~KPresenterChild();
  
  // get geometry, document and parent
  const QRect& geometry() {return m_geometry;}
  OPParts::Document_ptr document() {return OPParts::Document::_duplicate(m_rDoc);}
  KPresenterDocument_impl* parent() {return m_pKPresenterDoc;}

  // set the geometry
  void setGeometry(const QRect& _rect) {m_geometry = _rect;}
  
protected:

  // parent, document and geometry
  KPresenterDocument_impl *m_pKPresenterDoc;
  Document_ref m_rDoc;
  QRect m_geometry;

};

/*****************************************************************/
/* class KPresenterDocument_impl                                 */
/*****************************************************************/
class KPresenterDocument_impl : public QObject,
				virtual public Document_impl,
				virtual public KPresenter::KPresenterDocument_skel
{
  Q_OBJECT

public:

  // ------ C++ ------ 
  // constructor - destructor
  KPresenterDocument_impl();
  ~KPresenterDocument_impl();
  
  // clean
  virtual void cleanUp();
  
  // ------ IDL ------
  // open - save document
  virtual CORBA::Boolean open(const char *_filename);
  virtual CORBA::Boolean saveAs(const char *_filename,const char *_format);

  // create a view
  virtual OPParts::View_ptr createView();

  // get list of views
  virtual void viewList(OPParts::Document::ViewList*& _list);

  // get mime type
  virtual char* mimeType() {return CORBA::string_dup(MIME_TYPE);}
  
  // ask, if document is modified
  virtual CORBA::Boolean isModified() {return m_bModified;}
  
  // ------ C++ ------
  // get output- and unputformats
  virtual QStrList outputFormats();
  virtual QStrList inputFormats();

  // add - remove a view
  virtual void addView(KPresenterView_impl *_view);
  virtual void removeView(KPresenterView_impl *_view);
  
  // insert an object
  virtual void insertObject(const QRect& _rect);

  // change geomentry of a child
  virtual void changeChildGeometry(KPresenterChild *_child, const QRect&);
  
  // get iterator if a child
  virtual QListIterator<KPresenterChild> childIterator();
  
  // page layout
  void setPageLayout(KoPageLayout,int,int);
  KoPageLayout pageLayout() {return _pageLayout;}

  // insert a page
  unsigned int insertNewPage(int,int); 

  // get number of pages nad objects
  unsigned int getPageNums() {return _pageNums;}
  unsigned int objNums() {return _objNums;}

  // background
  void setBackColor(unsigned int,QColor);
  void setBackPic(unsigned int,const char*);
  void setBackClip(unsigned int,const char*);
  void setBPicView(unsigned int,BackView);
  void setBackType(unsigned int,BackType);
  bool setPenBrush(QPen,QBrush,int,int);
  BackType getBackType(unsigned int);
  BackView getBPicView(unsigned int);
  const char* getBackPic(unsigned int);
  const char* getBackClip(unsigned int);
  QColor getBackColor(unsigned int);
  QPen getPen(QPen);
  QBrush getBrush(QBrush);

  // raise and lower objs
  void raiseObjs(int,int);
  void lowerObjs(int,int);

  // insert/change objects
  void insertPicture(const char*,int,int);
  void insertClipart(const char*,int,int);
  void changeClipart(const char*,int,int);
  void insertLine(QPen,LineType,int,int);
  void insertRectangle(QPen,QBrush,RectType,int,int);
  void insertCircleOrEllipse(QPen,QBrush,int,int);
  void insertText(int,int);
  void insertAutoform(QPen,QBrush,const char*,int,int);
  
  // get list of pages and objects
  QList<Background> *pageList() {return &_pageList;}
  QList<PageObjects> *objList() {return &_objList;}

  // get raster
  unsigned int rastX() {return _rastX;}
  unsigned int rastY() {return _rastY;}

  // get options for editmodi
  QColor txtBackCol() {return _txtBackCol;}
  QColor txtSelCol() {return _txtSelCol;}

  // get values for screenpresentations
  bool spInfinitLoop() {return _spInfinitLoop;}
  bool spManualSwitch() {return _spManualSwitch;}
  QList<SpPageConfiguration> *spPageConfig() {return &_spPageConfig;}

  // size of page
  QRect getPageSize(unsigned int,int,int);

  // delete/rotate/rearrange/reorder obejcts
  void deleteObjs();
  void rotateObjs();
  void reArrangeObjs();

signals:

  // document modified
  void sig_KPresenterModified();

  // object inserted - removed
  void sig_insertObject(KPresenterChild *_child);
  void sig_removeObject(KPresenterChild *_child);

  // update child geometry
  void sig_updateChildGeometry(KPresenterChild *_child);

protected:

  // *********** functions ***********

  // repaint all views
  void repaint(bool);
  void repaint(unsigned int,unsigned int,unsigned int,unsigned int,bool);

  // *********** variables ***********

  // list of views and children
  QList<KPresenterView_impl> m_lstViews;
  QList<KPresenterChild> m_lstChildren;
  KPresenterView_impl *viewPtr;

  // modified?
  bool m_bModified;

  // page layout and background
  KoPageLayout _pageLayout;
  QList<Background> _pageList; 

  // list of objects
  QList<PageObjects> _objList;     

  // number of pages and objects
  unsigned int _pageNums;
  unsigned int _objNums;

  // screenpresentations
  QList<SpPageConfiguration> _spPageConfig;
  bool _spInfinitLoop,_spManualSwitch;

  // options
  int _rastX,_rastY;   
  int _xRnd,_yRnd;

  // options for editmode
  QColor _txtBackCol;
  QColor _txtSelCol;

  // pointers
  PageObjects *objPtr;
  Background *pagePtr;
  SpPageConfiguration *spPCPtr;
  
};

#endif




