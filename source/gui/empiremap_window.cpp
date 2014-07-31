// This file is part of CaesarIA.
//
// CaesarIA is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// CaesarIA is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with CaesarIA.  If not, see <http://www.gnu.org/licenses/>
//
// Copyright 2012-2014 Dalerank, dalerankn8@gmail.com

#include "empiremap_window.hpp"
#include "gfx/picturesarray.hpp"
#include "core/event.hpp"
#include "gfx/engine.hpp"
#include "texturedbutton.hpp"
#include "objects/dock.hpp"
#include "core/color.hpp"
#include "world/empire.hpp"
#include "world/computer_city.hpp"
#include "city/helper.hpp"
#include "label.hpp"
#include "core/stringhelper.hpp"
#include "core/gettext.hpp"
#include "dialogbox.hpp"
#include "good/goodstore.hpp"
#include "world/trading.hpp"
#include "city/funds.hpp"
#include "good/goodhelper.hpp"
#include "game/settings.hpp"
#include "events/showinfobox.hpp"
#include "core/logger.hpp"
#include "world/merchant.hpp"
#include "core/foreach.hpp"
#include "world/object.hpp"
#include "events/fundissue.hpp"
#include "events/showadvisorwindow.hpp"
#include "widgetescapecloser.hpp"
#include "gameautopause.hpp"
#include "gui/environment.hpp"

using namespace constants;
using namespace gfx;

namespace gui
{

static const char* empMapOffset = "EmpireMapWindowOffset";

class EmpireMapWindow::Impl
{
public:
  GameAutoPause autopause;
  PictureRef border;
  Picture empireMap;
  world::CityPtr currentCity;
  Point offset;
  bool dragging;
  Point dragStartPosition;
  Picture leftEagle, rightEagle;
  Size eagleOffset;
  Picture centerPicture;
  PushButton* btnHelp;
  PushButton* btnExit;
  PushButton* btnTrade;
  std::string ourCity;

  Label* lbCityTitle;
  Widget* tradeInfo;
  world::EmpirePtr empire;

  void checkCityOnMap( const Point& pos );
  void showOpenRouteRequestWindow();
  void createTradeRoute();
  void drawCityGoodsInfo();
  void drawTradeRouteInfo();
  void drawCityInfo();
  void resetInfoPanel();
  void updateCityInfo();
  void showTradeAdvisorWindow();
};

void EmpireMapWindow::Impl::checkCityOnMap( const Point& pos )
{
  world::CityList cities = empire->cities();

  currentCity = 0;
  foreach( city, cities )
  {
    Rect rect( (*city)->location(), Size( 40 ) );
    if( rect.isPointInside( pos ) )
    {
      currentCity = (*city);
      break;
    }
  }

  updateCityInfo();
}

void EmpireMapWindow::Impl::updateCityInfo()
{
  resetInfoPanel();
  if( currentCity != 0 )
  {
    lbCityTitle->setText( currentCity->name() );

    if( is_kind_of<PlayerCity>( currentCity ) )
    {
      drawCityInfo();
    }
    else
    {
      world::ComputerCityPtr ccity = ptr_cast<world::ComputerCity>( currentCity );
      if( ccity.isNull() )
        return;

      if( ccity->isDistantCity() || ccity->isRomeCity() )
      {

      }
      else
      {
        world::TraderoutePtr route = empire->findRoute( currentCity->name(), ourCity );
        if( route != 0 )
        {
          drawTradeRouteInfo();
        }
        else
        {
          drawCityGoodsInfo();
        }
      }
    }
  }
  else
  {
    lbCityTitle->setText( "" );    
  }
}

void EmpireMapWindow::Impl::showTradeAdvisorWindow()
{
  events::GameEventPtr e = events::ShowAdvisorWindow::create( true, advisor::trading );
  e->dispatch();
}

void EmpireMapWindow::Impl::createTradeRoute()
{
  if( currentCity != 0 )
  {
    unsigned int cost = world::EmpireHelper::getTradeRouteOpenCost( empire, ourCity, currentCity->name() );
    events::GameEventPtr e = events::FundIssueEvent::create( city::Funds::sundries, -(int)cost );
    e->dispatch();
    world::TraderoutePtr route = empire->createTradeRoute( ourCity, currentCity->name() );

    PlayerCityPtr plCity = ptr_cast<PlayerCity>( empire->findCity( ourCity ) );
    if( plCity.isValid() && route.isValid() && route->isSeaRoute() )
    {
      city::Helper helper( plCity );
      DockList docks = helper.find<Dock>( constants::building::dock );
      if( docks.empty() )
      {
        events::GameEventPtr e = events::ShowInfobox::create( _("##no_working_dock##" ), _( "##no_dock_for_sea_trade_routes##" ) );
        e->dispatch();
      }
    }
  }

  updateCityInfo();
}

void EmpireMapWindow::Impl::drawCityInfo()
{
  Label* lb = new Label( tradeInfo, Rect( Point( 0, tradeInfo->height() - 70), Size( tradeInfo->width(), 30) ) );
  lb->setTextAlignment( align::center, align::upperLeft );
  if( is_kind_of<PlayerCity>( currentCity ) )
  {
    lb->setText( _("##empiremap_our_city##") );
  }
  else if( is_kind_of<world::ComputerCity>(currentCity) )
  {
    SmartPtr<world::ComputerCity> compCity = ptr_cast<world::ComputerCity>( currentCity );
    if( compCity->isDistantCity() )
    {
      lb->setText( _("##empiremap_distant_city##") );
    }
  }
  /*else if( currentCity->isRomeCity() )
  {
    lb->setText( _("##empiremap_rome_city##") );
  }
  */
}

void EmpireMapWindow::Impl::drawCityGoodsInfo()
{
  Point startInfo( 0, 0 );
  Point startButton( 0, 40 );


  Point startDraw( (tradeInfo->width() - 400) / 2, tradeInfo->height() - 90 );
  new Label( tradeInfo, Rect( startDraw + startInfo, Size( 70, 30 )), _("##emw_sell##") );

  const GoodStore& sellgoods = currentCity->importingGoods();
  for( int i=0, k=0; i < Good::goodCount; i++ )
  {
    if( sellgoods.capacity( (Good::Type)i ) > 0  )
    {
      Label* lb = new Label( tradeInfo, Rect( startDraw + startInfo + Point( 30 * (k+2), 0 ), Size( 24, 24 ) ) );
      lb->setBackgroundPicture( GoodHelper::getPicture( Good::Type(i), true) );
      lb->setTooltipText( GoodHelper::getTypeName( Good::Type(i) ) );
      k++;
    }
  }

  Point buyPoint = startDraw + Point( 200, 0 );
  new Label( tradeInfo, Rect( buyPoint + startInfo, Size( 70, 30 )), _("##emw_buy##") );

  const GoodStore& buygoods = currentCity->exportingGoods();
  for( int i=0, k=0; i < Good::goodCount; i++ )
  {
    if( buygoods.capacity( (Good::Type)i ) > 0  )
    {
      Label* lb = new Label( tradeInfo, Rect( buyPoint + startInfo + Point( 30 * (k+2), 0 ), Size( 24, 24 ) ) );
      lb->setBackgroundPicture(  GoodHelper::getPicture( Good::Type(i), true) );
      lb->setTooltipText( GoodHelper::getTypeName( Good::Type(i) ) );
      k++;
    }
  }

  PushButton* btnOpenTrade = new PushButton( tradeInfo, Rect( startDraw + startButton, Size( 400, 20 ) ),
                                             "", -1, false, PushButton::blackBorderUp );

  unsigned int routeOpenCost = world::EmpireHelper::getTradeRouteOpenCost( empire, ourCity, currentCity->name() );

  btnOpenTrade->setText( StringHelper::format( 0xff, "%d %s", routeOpenCost, _("##dn_for_open_trade##")));

  CONNECT( btnOpenTrade, onClicked(), this, Impl::showOpenRouteRequestWindow );
}

void EmpireMapWindow::Impl::drawTradeRouteInfo()
{
  Point startDraw( (tradeInfo->width() - 400) / 2, tradeInfo->height() - 80 );
  new Label( tradeInfo, Rect( startDraw, Size( 80, 30 )), _("##emw_sold##") );

  const GoodStore& sellgoods = currentCity->importingGoods();
  for( int i=0, k=0; i < Good::goodCount; i++ )
  {
    int maxsell = sellgoods.capacity( (Good::Type)i ) / 100;
    int cursell = sellgoods.qty( (Good::Type)i ) / 100;
    if( maxsell > 0  )
    {
      Label* lb = new Label( tradeInfo, Rect( startDraw + Point( 80 + 100 * k, 0 ), Size( 24, 24 ) ) );
      lb->setBackgroundPicture(  GoodHelper::getPicture( Good::Type(i), true) );

      std::string text = StringHelper::format( 0xff, "%d/%d", cursell, maxsell );
      new Label( tradeInfo, Rect( startDraw + Point( 110 + 100 * k, 0), Size( 70, 30 ) ), text );
      k++;
    }
  }

  Point buyPoint = startDraw + Point( 0, 30 );
  new Label( tradeInfo, Rect( buyPoint, Size( 80, 30 )), _("##emw_bought##") );

  const GoodStore& buygoods = currentCity->exportingGoods();
  for( int i=0, k=0; i < Good::goodCount; i++ )
  {
    int maxbuy = buygoods.capacity( (Good::Type)i ) / 100;
    int curbuy = buygoods.qty( (Good::Type)i ) / 100;
    if( maxbuy > 0  )
    {
      Label* lb = new Label( tradeInfo, Rect( buyPoint + Point( 80 + 100 * k, 0 ), Size( 24, 24 ) ) );
      lb->setBackgroundPicture( GoodHelper::getPicture( Good::Type(i), true) );

      std::string text = StringHelper::format( 0xff, "%d/%d", curbuy, maxbuy );
      new Label( tradeInfo, Rect( buyPoint + Point( 110 + 100 * k, 0), Size( 70, 30 ) ), text );
      k++;
    }
  }
}

void EmpireMapWindow::Impl::resetInfoPanel()
{
  Widget::Widgets childs = tradeInfo->children();
  foreach( widget, childs ) { (*widget)->deleteLater(); }
}

void EmpireMapWindow::Impl::showOpenRouteRequestWindow()
{
  DialogBox* dialog = new DialogBox( tradeInfo->environment()->rootWidget(), Rect( 0, 0, 0, 0 ),
                                     _("##emp_open_trade_route##"), _("##emp_pay_open_this_route_question##"), 
                                     DialogBox::btnOk | DialogBox::btnCancel  );

  CONNECT( dialog, onOk(), this, Impl::createTradeRoute );
  CONNECT( dialog, onOk(), dialog, DialogBox::deleteLater );
  CONNECT( dialog, onCancel(), dialog, DialogBox::deleteLater );
}

EmpireMapWindow::EmpireMapWindow( Widget* parent, int id )
 : Widget( parent, id, Rect( Point(0, 0), parent->size() ) ), _d( new Impl )
{
  // use some clipping to remove the right and bottom areas
  _d->autopause.activate();
  _d->border.reset( Picture::create( size() ) );
  _d->empireMap = Picture::load( "the_empire", 1 );
  _d->dragging = false;
  _d->lbCityTitle = new Label( this, Rect( Point( (width() - 240) / 2 + 60, height() - 132 ), Size( 240, 32 )) );
  _d->lbCityTitle->setFont( Font::create( FONT_3 ) );
  _d->offset = GameSettings::get( empMapOffset ).toPoint();

  WidgetEscapeCloser::insertTo( this );

  _d->tradeInfo = new Widget( this, -1, Rect( 0, height() - 120, width(), height() ) );

  _d->border->lock();
  Picture backgr = Picture::load( ResourceGroup::empirepnls, 4 );
  for( unsigned int y=height() - 120; y < height(); y+=backgr.height() )
  {
    for( unsigned int x=0; x < width(); x += backgr.width() )
    {
      _d->border->draw( backgr, x, y );
    }
  }

  Picture lrBorderPic = Picture::load( ResourceGroup::empirepnls, 1 );
  for( unsigned int y = 0; y < height(); y += lrBorderPic.height() )
  {
    _d->border->draw( lrBorderPic, 0, y );
    _d->border->draw( lrBorderPic, width() - lrBorderPic.width(), y );
  }

  Picture tdBorderPic = Picture::load( ResourceGroup::empirepnls, 2 );
  for( unsigned int x = 0; x < width(); x += tdBorderPic.width() )
  {
    _d->border->draw( tdBorderPic, x, 0 );
    _d->border->draw( tdBorderPic, x, height() - tdBorderPic.height() );
    _d->border->draw( tdBorderPic, x, height() - 120 );
  }

  Picture corner = Picture::load( ResourceGroup::empirepnls, 3 );
  _d->border->draw( corner, 0, 0 );    //left top
  _d->border->draw( corner, 0, height() - corner.height() ); //top right
  _d->border->draw( corner, width() - corner.width(), 0 ); //left bottom
  _d->border->draw( corner, width() - corner.width(), height() - corner.height() ); //bottom right
  _d->border->draw( corner, 0, height() - 120 ); //left middle
  _d->border->draw( corner, width() - corner.width(), height() - 120 ); //right middle

  _d->border->fill( 0x00000000, Rect( corner.width(), corner.height(), 
                                      width() - corner.width(), height() - 120 ) );
  _d->border->unlock();

  _d->leftEagle = Picture::load( ResourceGroup::empirepnls, 7 );
  _d->rightEagle = Picture::load( ResourceGroup::empirepnls, 8 );
  _d->eagleOffset = corner.size();

  _d->centerPicture = Picture::load( ResourceGroup::empirepnls, 9 );

  _d->btnHelp = new TexturedButton( this, Point( 20, height() - 44 ), Size( 24 ), -1, 528 );
  _d->btnExit = new TexturedButton( this, Point( width() - 44, height() - 44 ), Size( 24 ), -1, 533 );
  _d->btnTrade = new TexturedButton( this, Point( width() - 48, height() - 100), Size( 28 ), -1, 292 );
  _d->btnTrade->setTooltipText( _("##to_trade_advisor##") );

  CONNECT( _d->btnExit, onClicked(), this, EmpireMapWindow::deleteLater );
  CONNECT( _d->btnTrade, onClicked(), this, EmpireMapWindow::deleteLater );
  CONNECT( _d->btnTrade, onClicked(), _d.data(), Impl::showTradeAdvisorWindow );
}

void EmpireMapWindow::draw(gfx::Engine& engine )
{
  if( !visible() )
    return;

  engine.draw( _d->empireMap, _d->offset );  

  //draw static objects
  foreach( obj, _d->empire->objects() )
  {
    if( !(*obj)->isMovable() )
    {
      engine.draw( (*obj)->pictures(), _d->offset + (*obj)->location() );
    }
  }

  world::CityList cities = _d->empire->cities();
  foreach( it, cities )
  {
    Point location = (*it)->location();
    engine.draw( (*it)->pictures(), _d->offset + location );
  }

  world::TraderouteList routes = _d->empire->tradeRoutes();
  foreach( it, routes )
  {
    world::TraderoutePtr route = *it;

    const PointsArray& points = route->points();
    const Pictures& pictures = route->pictures();

    for( unsigned int index=0; index < pictures.size(); index++ )
    {
      engine.draw( pictures[ index ], _d->offset + points[ index ] );
    }

    world::MerchantPtr merchant = route->merchant( 0 );
    if( merchant != 0 )
    {
      engine.draw( merchant->picture(), _d->offset + merchant->location() );
    }      
  }

  //draw movable objects
  foreach( obj, _d->empire->objects() )
  {
    if( (*obj)->isMovable() )
    {
      engine.draw( (*obj)->pictures(), _d->offset + (*obj)->location() );
    }
  }

  engine.draw( *_d->border, Point( 0, 0 ) );

  engine.draw( _d->leftEagle, _d->eagleOffset.width(), height() - 120 + _d->eagleOffset.height() - _d->leftEagle.height() - 10 );
  engine.draw( _d->rightEagle, width() - _d->eagleOffset.width() - _d->rightEagle.width(),
                      height() - 120 + _d->eagleOffset.height() - _d->rightEagle.height() - 10 );

  engine.draw( _d->centerPicture, (width() - _d->centerPicture.width()) / 2,
                      height() - 120 - _d->centerPicture.height() + 20 );

  Widget::draw( engine );
}

bool EmpireMapWindow::onEvent( const NEvent& event )
{
  if( event.EventType == sEventMouse )
  {
    switch(event.mouse.type)
    {
    case mouseLbtnPressed:
      _d->dragStartPosition = event.mouse.pos();
      _d->dragging = true;//_d->flags.isFlag( draggable );
      bringToFront();

      _d->checkCityOnMap( _d->dragStartPosition - _d->offset );
    break;

    case mouseRbtnRelease:
      deleteLater();
      _d->dragging = false;
    break;

    case mouseLbtnRelease:
      _d->dragging = false;
    break;

    case mouseMoved:
      {
        //bool t = _d->dragging;

        if ( !event.mouse.isLeftPressed() )
        {
          _d->dragging = false;
        }

        if( _d->dragging )
        {
          // gui window should not be dragged outside its parent
          if( _d->offset.x() > 0
              || _d->offset.x() + _d->empireMap.width() < (int)width()
              || _d->offset.y() > 0
              || _d->offset.y() + _d->empireMap.height() < (int)height()-120 )
          {
            break;
          }

          _d->offset += (event.mouse.pos() - _d->dragStartPosition);
          _d->dragStartPosition = event.mouse.pos();

          _d->offset.setX( math::clamp<int>( _d->offset.x(), -_d->empireMap.width() + width(), 0 ) );
          _d->offset.setY( math::clamp<int>( _d->offset.y(), -_d->empireMap.height() + height() - 120, 0 ) );
        }
      }
    break;
    
    default:
    break;
    }

    return true;
  }

  return Widget::onEvent( event );
}

EmpireMapWindow* EmpireMapWindow::create(world::EmpirePtr empire, PlayerCityPtr city, Widget* parent, int id )
{
  EmpireMapWindow* ret = new EmpireMapWindow( parent, id );
  ret->_d->empire = empire;
  ret->_d->ourCity = city->name();

  return ret;
}

EmpireMapWindow::~EmpireMapWindow()
{
  GameSettings::set( empMapOffset, _d->offset );
}

}//end namespace gui
