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
// Copyright 2012-2014 Gregoire Athanase, gathanase@gmail.com
// Copyright 2012-2014 Dalerank, dalerank@gmail.com

#include "logo.hpp"

#include "gfx/engine.hpp"
#include "core/exception.hpp"
#include "gfx/picture.hpp"
#include "gfx/pictureconverter.hpp"
#include "core/color.hpp"
#include "core/font.hpp"
#include "core/gettext.hpp"

using namespace gfx;

namespace scene
{

class SplashScreen::Impl
{
public:
  Picture background;
  PictureRef textPic;
  std::string text, prefix;
  PictureRef fadetx;

public:
  void fade(Engine &engine, Picture &pic, bool out, int offset);
};

SplashScreen::SplashScreen() : _d( new Impl ) {}

SplashScreen::~SplashScreen() {}

void SplashScreen::initialize()
{
  Engine& engine = Engine::instance();

  _d->background = Picture::load("logo", 1);

  // center the background on the screen
  Size s = (engine.screenSize() - _d->background.size()) / 2;
  _d->background.setOffset( Point( s.width(), -s.height() ) );

  _d->textPic.init( Size( _d->background.width(), 30 ) );
  _d->textPic->setOffset( Point( (engine.screenSize().width() - _d->textPic->width()) / 2,
                                  _d->background.offset().y() - _d->background.height() - 5 ) );

  _d->fadetx.init( engine.screenSize() );
  _d->fadetx->fill( NColor(0xff, 0, 0, 0), Rect() );
}

void SplashScreen::draw()
{
  Engine& engine = Engine::instance();

  engine.draw( _d->background, 0, 0);

  if( !_d->text.empty() )
  {
    Font textFont = Font::create( FONT_2_WHITE ) ;

    Rect textRect = textFont.getTextRect( _d->text, Rect( Point(), _d->textPic->size() ), align::center, align::center );

    _d->textPic->fill( 0xff000000 );

    textFont.draw( *_d->textPic, _d->text, textRect.left(), textRect.top(), false, true );

    engine.draw( *_d->textPic, 0, 0 );
  }
}

void SplashScreen::Impl::fade( Engine& engine, Picture& pic, bool out, int offset )
{
  int start = out ? 0 : 0xff;
  int stop = out ? 0xff : 0;
  offset *= (out ? 1 : -1);

  for( int k=start; out ? k < stop : k > stop ; k+=offset )
  {
    engine.startRenderFrame();
    //fadetx->fill( NColor(k, 0, 0, 0), Rect() );
    fadetx->setAlpha( k );
    fadetx->update();
    engine.draw( pic, 0, 0);
    engine.draw( *fadetx, 0, 0);
    engine.endRenderFrame();
  }
}

void SplashScreen::exitScene()
{
//#ifndef DEBUG
  Engine& engine = Engine::instance();

  int offset = 3;

#ifdef CAESARIA_PLATFORM_ANDROID
  offset = 12;
#endif

  _d->fade( engine, _d->background, true, offset );

  _d->textPic.init( engine.screenSize() );
  _d->textPic->fill( 0xff000000, Rect( Point( 0, 0 ), _d->textPic->size() ) );

  Font textFont = Font::create( FONT_3 ) ;

  std::string text[5] = { "This is an early access version of CaesarIA!",
                          "The outer reaches are placeholders: more features and polish are coming soon",
                          "see",
                          "www.bitbucket.org/dalerank/caesaria",
                          "for our roadmap." };
  for( int i=0; i < 5; i++ )
  {
    Rect textRect = textFont.getTextRect( text[i], Rect( Point(), _d->textPic->size() ), align::center, align::center );
    textFont.setColor( i == 3 ?  DefaultColors::indianRed : DefaultColors::dodgerBlue );
    textFont.draw( *_d->textPic, text[i], textRect.left(), textRect.top() + 20 * i, false, true );
  }

  _d->fade( engine, *_d->textPic, false, offset );
  engine.delay( 3000 );
  _d->fade( engine, *_d->textPic, true, offset );
//#endif
}

void SplashScreen::setText(std::string text)
{
  _d->text = _d->prefix + " " + _( text );

  Engine& engine = Engine::instance();
  update( engine );
}

void SplashScreen::setPrefix(std::string prefix) { _d->prefix = _( prefix ); }
int SplashScreen::result() const { return 0; }

}//end namespace scene
