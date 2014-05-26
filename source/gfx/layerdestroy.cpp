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
// along with CaesarIA.  If not, see <http://www.gnu.org/licenses/>.
//
// Copyright 2012-2013 Dalerank, dalerankn8@gmail.com

#include "layerdestroy.hpp"
#include "layerconstants.hpp"
#include "events/event.hpp"
#include "walker/constants.hpp"
#include "walker/walker.hpp"
#include "core/foreach.hpp"
#include "tilemap_camera.hpp"
#include "city/city.hpp"
#include "core/event.hpp"
#include "core/stringhelper.hpp"
#include "objects/metadata.hpp"
#include "events/fundissue.hpp"
#include "city/funds.hpp"

using namespace constants;

namespace gfx
{

namespace {
const unsigned int tilePosHashValue = 10000;
}

void LayerDestroy::_clearAll()
{
  TilesArray tiles4clear = _getSelectedArea();
  foreach( tile, tiles4clear )
  {
    events::GameEventPtr event = events::ClearLandEvent::create( (*tile)->pos() );
    event->dispatch();
  }
}

unsigned int LayerDestroy::_checkMoney4destroy(const Tile& tile)
{
  TileOverlayPtr overlay = tile.overlay();
  if( overlay.isValid() )
  {
    const MetaData& mdata = MetaDataHolder::getData( overlay->type() );
    return mdata.getOption( MetaDataOptions::cost ).toInt() / 2;
  }

  if( tile.getFlag( Tile::tlTree ) ) return 6;
  if( tile.getFlag( Tile::tlRoad) ) return 4;

  return 0;
}

void LayerDestroy::_drawTileInSelArea( Engine& engine, Tile& tile, Tile* master, const Point& offset )
{
  if( master==NULL )
  {
    // single-tile
    drawTile( engine, tile, offset );
    engine.draw( _clearPic, tile.mapPos() + offset );
  }
  else
  {
    if( master->getFlag( Tile::isDestructible ) )
    {
      engine.setTileDrawMask( 0x00ff0000, 0, 0, 0xff000000 );
    }

    // multi-tile: draw the master tile.
    if( !master->getFlag( Tile::wasDrawn ) )
      drawTile( engine, *master, offset );

    engine.resetTileDrawMask();
  }
}

inline unsigned int __tpHash( const TilePos& pos )
{
  return (pos.j() * tilePosHashValue + pos.i());
}

void LayerDestroy::render( Engine& engine )
{
  // center the map on the screen
  Point cameraOffset = _camera()->getOffset();

  _camera()->startFrame();

  const TilesArray& visibleTiles = _camera()->getTiles();

  Tilemap& tmap = _city()->tilemap();

  std::set<int> hashDestroyArea;
  TilesArray destroyArea = _getSelectedArea();

  //create list of destroy tiles add full area building if some of it tile constain in destroy area
  unsigned int saveSum = _money4destroy;
  _money4destroy = 0;
  foreach( it, destroyArea)
  {
    Tile* tile = *it;
    hashDestroyArea.insert( __tpHash( tile->pos() ) );

    TileOverlayPtr overlay = tile->overlay();
    if( overlay.isValid() )
    {
      TilesArray overlayArea = tmap.getArea( overlay->pos(), overlay->size() );
      foreach( ovelayTile, overlayArea )
      {
        hashDestroyArea.insert( __tpHash( (*ovelayTile)->pos() ) );
      }
    }

    _money4destroy += _checkMoney4destroy( *tile );
  }

  // FIRST PART: draw all flat land (walkable/boatable)
  foreach( it, visibleTiles )
  {
    Tile* tile = *it;
    Tile* master = tile->masterTile();

    if( !tile->isFlat() )
      continue;

    int tilePosHash = __tpHash( tile->pos() );
    if( hashDestroyArea.find( tilePosHash ) != hashDestroyArea.end() )
    {
      _drawTileInSelArea( engine, *tile, master, cameraOffset );
    }
    else
    {
      if( master==NULL )
      {
        // single-tile
        drawTile( engine, *tile, cameraOffset );
      }
      else if( !master->getFlag( Tile::wasDrawn ) )
      {
        // multi-tile: draw the master tile.
        drawTile( engine, *master, cameraOffset );
      }
    }
  }

  // SECOND PART: draw all sprites, impassable land and buildings
  foreach( it, visibleTiles )
  {
    Tile* tile = *it;
    int z = tile->pos().z();

    int tilePosHash = __tpHash( tile->pos() );
    if( hashDestroyArea.find( tilePosHash ) != hashDestroyArea.end() )
    {
      if( tile->getFlag( Tile::isDestructible ) )
      {
        engine.setTileDrawMask( 0x00ff0000, 0, 0, 0xff000000 );
      }
    }

    drawTileR( engine, *tile, cameraOffset, z, false );

    _drawWalkers( engine, *tile, cameraOffset );
    engine.resetTileDrawMask();
  }

  if( saveSum != _money4destroy )
  {
    _textPic->fill( 0x0, Rect() );
    _textFont.setColor( 0xffff0000 );
    _textFont.draw( *_textPic, StringHelper::i2str( _money4destroy ) + " Dn", Point() );
  }

  engine.draw( *_textPic, engine.cursorPos() + Point( 10, 10 ));
}

void LayerDestroy::handleEvent(NEvent& event)
{
  if( event.EventType == sEventMouse )
  {
    switch( event.mouse.type  )
    {
    case mouseMoved:
    {
      _setLastCursorPos( event.mouse.pos() );
      if( !event.mouse.isLeftPressed() || _startCursorPos().x() < 0 )
      {
        _setStartCursorPos( _lastCursorPos() );
      }
    }
    break;

    case mouseLbtnPressed:
    {
      _setStartCursorPos( _lastCursorPos() );
    }
    break;

    case mouseLbtnRelease:            // left button
    {
      Tile* tile = _camera()->at( event.mouse.pos(), false );  // tile under the cursor (or NULL)
      if( tile == 0 )
      {
        break;
      }

      _clearAll();
      _setStartCursorPos( _lastCursorPos() );
      events::GameEventPtr e = events::FundIssueEvent::create( city::Funds::buildConstruction, -_money4destroy );
      e->dispatch();
    }
    break;

    case mouseRbtnRelease:
    {
      _setNextLayer( citylayer::simple );
    }
    break;

    default:
    break;
    }
  }

  if( event.EventType == sEventKeyboard )
  {
    bool pressed = event.keyboard.pressed;
    int moveValue = _camera()->getScrollSpeed() * ( event.keyboard.shift ? 4 : 1 ) * (pressed ? 1 : 0);

    switch( event.keyboard.key )
    {
    case KEY_UP:    _camera()->moveUp   ( moveValue ); break;
    case KEY_DOWN:  _camera()->moveDown ( moveValue ); break;
    case KEY_RIGHT: _camera()->moveRight( moveValue ); break;
    case KEY_LEFT:  _camera()->moveLeft ( moveValue ); break;
    case KEY_ESCAPE: _setNextLayer( citylayer::simple ); break;
    default: break;
    }
  }
}

int LayerDestroy::type() const {  return citylayer::destroy; }

std::set<int> LayerDestroy::getVisibleWalkers() const
{
  std::set<int> ret;
  ret.insert( walker::all );

  return ret;
}

void LayerDestroy::drawTile( Engine& engine, Tile& tile, Point offset )
{
  TileOverlayPtr overlay = tile.overlay();

  if( overlay.isValid() )
  {
    registerTileForRendering( tile );
  }

  Layer::drawTile( engine, tile, offset );
}

LayerPtr LayerDestroy::create( Camera& camera, PlayerCityPtr city)
{
  LayerPtr ret( new LayerDestroy( camera, city ) );
  ret->drop();

  return ret;
}

LayerDestroy::LayerDestroy( Camera& camera, PlayerCityPtr city)
  : Layer( &camera, city )
{
  _clearPic = Picture::load( "oc3_land", 2 );
  _textFont = Font::create( FONT_3 );
  _textPic.init( Size( 100, 30 ) );
}

}//end namespace gfx
