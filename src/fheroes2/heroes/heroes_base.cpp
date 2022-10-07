/***************************************************************************
 *   fheroes2: https://github.com/ihhub/fheroes2                           *
 *   Copyright (C) 2019 - 2022                                             *
 *                                                                         *
 *   Free Heroes2 Engine: http://sourceforge.net/projects/fheroes2         *
 *   Copyright (C) 2010 by Andrey Afletdinov <fheroes2@gmail.com>          *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#include <algorithm>
#include <array>
#include <cassert>

#include "army.h"
#include "castle.h"
#include "heroes_base.h"
#include "kingdom.h"
#include "race.h"
#include "serialize.h"
#include "settings.h"
#include "spell_info.h"
#include "tools.h"
#include "translations.h"
#include "world.h"

HeroBase::HeroBase( const int type, const int race )
    : magic_point( 0 )
    , move_point( 0 )
{
    bag_artifacts.assign( HEROESMAXARTIFACT, Artifact::UNKNOWN );
    LoadDefaults( type, race );
}

void HeroBase::LoadDefaults( const int type, const int race )
{
    if ( Race::ALL & race ) {
        // fixed default primary skills
        Skill::Primary::LoadDefaults( type, race );

        // fixed default spell
        switch ( type ) {
        case HeroBase::CAPTAIN: {
            // force add spell book
            SpellBookActivate();

            const Spell spell = Skill::Primary::GetInitialSpell( race );
            if ( spell.isValid() )
                spell_book.Append( spell );
            break;
        }

        case HeroBase::HEROES: {
            const Spell spell = Skill::Primary::GetInitialSpell( race );
            if ( spell.isValid() ) {
                SpellBookActivate();
                spell_book.Append( spell );
            }
            break;
        }

        default:
            break;
        }
    }
}

HeroBase::HeroBase()
    : magic_point( 0 )
    , move_point( 0 )
{}

bool HeroBase::isCaptain() const
{
    return GetType() == CAPTAIN;
}

bool HeroBase::isHeroes() const
{
    return GetType() == HEROES;
}

uint32_t HeroBase::GetSpellPoints() const
{
    return magic_point;
}

void HeroBase::SetSpellPoints( const uint32_t points )
{
    magic_point = points;
}

bool HeroBase::isPotentSpellcaster() const
{
    // With knowledge 5 or less there isn't enough spell points to make a difference
    if ( knowledge <= 5 )
        return false;

    for ( const Spell & spell : spell_book ) {
        // This list is based on spells AI can use efficiently - should be updated later on
        switch ( spell.GetID() ) {
        case Spell::BLIND:
        case Spell::PARALYZE:
        case Spell::DIMENSIONDOOR:
        case Spell::SUMMONAELEMENT:
        case Spell::SUMMONEELEMENT:
        case Spell::SUMMONFELEMENT:
        case Spell::SUMMONWELEMENT:
        case Spell::MIRRORIMAGE:
            return true;
        case Spell::COLDRAY:
        case Spell::LIGHTNINGBOLT:
        case Spell::CHAINLIGHTNING:
        case Spell::METEORSHOWER:
        case Spell::ARMAGEDDON:
            if ( power > 5 )
                return true;
            break;
        case Spell::RESURRECT:
        case Spell::RESURRECTTRUE:
            if ( !GetArmy().AllTroopsAreUndead() )
                return true;
            break;
        case Spell::ANIMATEDEAD:
            if ( GetArmy().AllTroopsAreUndead() )
                return true;
            break;
        default:
            break;
        }
    }
    return false;
}

bool HeroBase::HaveSpellPoints( const Spell & spell ) const
{
    return magic_point >= spell.spellPoints( this );
}

bool HeroBase::haveMovePoints( const Spell & spell ) const
{
    return move_point >= spell.minMovePoints();
}

void HeroBase::EditSpellBook()
{
    spell_book.Edit( *this );
}

Spell HeroBase::OpenSpellBook( const SpellBook::Filter filter, const bool canCastSpell, const bool restorePreviousState,
                               const std::function<void( const std::string & )> * statusCallback ) const
{
    return spell_book.Open( *this, filter, canCastSpell, restorePreviousState, statusCallback );
}

SpellStorage HeroBase::getAllSpells() const
{
    SpellStorage storage;
    storage.Append( spell_book );
    storage.Append( bag_artifacts );

    return storage;
}

bool HeroBase::HaveSpell( const Spell & spell, const bool skip_bag ) const
{
    return HaveSpellBook() && ( spell_book.isPresentSpell( spell ) || ( !skip_bag && bag_artifacts.ContainSpell( spell.GetID() ) ) );
}

void HeroBase::AppendSpellToBook( const Spell & spell, const bool without_wisdom )
{
    if ( without_wisdom || CanLearnSpell( spell ) )
        spell_book.Append( spell );
}

void HeroBase::AppendSpellsToBook( const SpellStorage & spells, const bool without_wisdom )
{
    for ( const auto & spell : spells )
        AppendSpellToBook( spell, without_wisdom );
}

bool HeroBase::SpellBookActivate()
{
    return !HaveSpellBook() && bag_artifacts.PushArtifact( Artifact::MAGIC_BOOK );
}

bool HeroBase::hasArtifact( const Artifact & art ) const
{
    return bag_artifacts.isPresentArtifact( art );
}

int HeroBase::GetAttackModificator( std::string * strs ) const
{
    int result = 0;
    if ( strs == nullptr ) {
        result += bag_artifacts.getTotalArtifactEffectValue( fheroes2::ArtifactBonusType::ATTACK_SKILL );
    }
    else {
        result += bag_artifacts.getTotalArtifactEffectValue( fheroes2::ArtifactBonusType::ATTACK_SKILL, *strs );
    }

    // check castle modificator
    const Castle * castle = inCastle();

    if ( castle )
        result += castle->GetAttackModificator( strs );

    return result;
}

int HeroBase::GetDefenseModificator( std::string * strs ) const
{
    int result = 0;
    if ( strs == nullptr ) {
        result += bag_artifacts.getTotalArtifactEffectValue( fheroes2::ArtifactBonusType::DEFENCE_SKILL );
    }
    else {
        result += bag_artifacts.getTotalArtifactEffectValue( fheroes2::ArtifactBonusType::DEFENCE_SKILL, *strs );
    }

    // check castle modificator
    const Castle * castle = inCastle();

    if ( castle )
        result += castle->GetDefenseModificator( strs );

    return result;
}

int HeroBase::GetPowerModificator( std::string * strs ) const
{
    int result = 0;
    if ( strs == nullptr ) {
        result += bag_artifacts.getTotalArtifactEffectValue( fheroes2::ArtifactBonusType::SPELL_POWER_SKILL );
        result -= bag_artifacts.getTotalArtifactEffectValue( fheroes2::ArtifactCurseType::SPELL_POWER_SKILL );
    }
    else {
        result += bag_artifacts.getTotalArtifactEffectValue( fheroes2::ArtifactBonusType::SPELL_POWER_SKILL, *strs );
        result -= bag_artifacts.getTotalArtifactEffectValue( fheroes2::ArtifactCurseType::SPELL_POWER_SKILL, *strs );
    }

    // check castle modificator
    const Castle * castle = inCastle();

    if ( castle )
        result += castle->GetPowerModificator( strs );

    return result;
}

int HeroBase::GetKnowledgeModificator( std::string * strs ) const
{
    int result = 0;
    if ( strs == nullptr ) {
        result += bag_artifacts.getTotalArtifactEffectValue( fheroes2::ArtifactBonusType::KNOWLEDGE_SKILL );
    }
    else {
        result += bag_artifacts.getTotalArtifactEffectValue( fheroes2::ArtifactBonusType::KNOWLEDGE_SKILL, *strs );
    }

    // check castle modificator
    const Castle * castle = inCastle();

    if ( castle )
        result += castle->GetKnowledgeModificator( strs );

    return result;
}

int HeroBase::GetMoraleModificator( std::string * strs ) const
{
    int result = 0;

    // army modificator (including the castle modificator)
    result += GetArmy().GetMoraleModificator( strs );

    if ( strs == nullptr ) {
        result += bag_artifacts.getTotalArtifactEffectValue( fheroes2::ArtifactBonusType::MORALE );
        if ( Modes( Heroes::SHIPMASTER ) ) {
            result += bag_artifacts.getTotalArtifactEffectValue( fheroes2::ArtifactBonusType::SEA_BATTLE_MORALE_BOOST );
        }

        result -= bag_artifacts.getTotalArtifactEffectValue( fheroes2::ArtifactCurseType::MORALE );
    }
    else {
        result += bag_artifacts.getTotalArtifactEffectValue( fheroes2::ArtifactBonusType::MORALE, *strs );
        if ( Modes( Heroes::SHIPMASTER ) ) {
            result += bag_artifacts.getTotalArtifactEffectValue( fheroes2::ArtifactBonusType::SEA_BATTLE_MORALE_BOOST, *strs );
        }

        result -= bag_artifacts.getTotalArtifactEffectValue( fheroes2::ArtifactCurseType::MORALE, *strs );
    }

    return result;
}

int HeroBase::GetLuckModificator( std::string * strs ) const
{
    int result = 0;

    // army modificator (including the castle modificator)
    result += GetArmy().GetLuckModificator( strs );

    if ( strs == nullptr ) {
        result += bag_artifacts.getTotalArtifactEffectValue( fheroes2::ArtifactBonusType::LUCK );
        if ( Modes( Heroes::SHIPMASTER ) ) {
            result += bag_artifacts.getTotalArtifactEffectValue( fheroes2::ArtifactBonusType::SEA_BATTLE_LUCK_BOOST );
        }
    }
    else {
        result += bag_artifacts.getTotalArtifactEffectValue( fheroes2::ArtifactBonusType::LUCK, *strs );
        if ( Modes( Heroes::SHIPMASTER ) ) {
            result += bag_artifacts.getTotalArtifactEffectValue( fheroes2::ArtifactBonusType::SEA_BATTLE_LUCK_BOOST, *strs );
        }
    }

    return result;
}

double HeroBase::GetMagicStrategicValue( const double armyStrength ) const
{
    const SpellStorage spells = getAllSpells();
    const uint32_t currentSpellPoints = GetSpellPoints();
    const int spellPower = GetPower();

    double bestValue = 0;
    for ( const Spell & spell : spells ) {
        if ( spell.isCombat() ) {
            bestValue = std::max( bestValue, spell.getStrategicValue( armyStrength, currentSpellPoints, spellPower ) );
        }
    }

    return bestValue;
}

bool HeroBase::CanCastSpell( const Spell & spell, std::string * res /* = nullptr */ ) const
{
    if ( !HaveSpellBook() ) {
        if ( res ) {
            // This should not happen for a human-controlled hero (for which this method is usually called with the non-null res)
            assert( 0 );
            *res = _( "Spell book is not present." );
        }
        return false;
    }

    if ( !HaveSpellPoints( spell ) ) {
        if ( res ) {
            *res = _( "That spell costs %{mana} mana. You only have %{point} mana, so you can't cast the spell." );
        }
        return false;
    }

    if ( !HaveSpell( spell ) ) {
        if ( res ) {
            // This should not happen for a human-controlled hero (for which this method is usually called with the non-null res)
            assert( 0 );
            *res = _( "The spell was not found." );
        }
        return false;
    }

    if ( spell.isAdventure() ) {
        const Heroes * hero = dynamic_cast<const Heroes *>( this );
        if ( hero == nullptr ) {
            // How is it possible that a captain can access this spell?
            assert( 0 );
            if ( res != nullptr ) {
                *res = _( "Only heroes can cast this spell." );
            }
            return false;
        }

        if ( !haveMovePoints( spell ) ) {
            if ( res ) {
                *res = _( "Your hero is too tired to cast this spell today. Try again tomorrow." );
            }
            return false;
        }

        if ( spell == Spell::TOWNGATE || spell == Spell::TOWNPORTAL ) {
            const KingdomCastles & castles = hero->GetKingdom().GetCastles();
            bool hasCastles = std::any_of( castles.begin(), castles.end(), []( const Castle * castle ) { return castle && !castle->GetHeroes().Guest(); } );
            if ( !hasCastles ) {
                if ( res != nullptr ) {
                    *res = _( "You do not currently own any town or castle, so you can't cast the spell." );
                }
                return false;
            }
        }

        if ( spell == Spell::TOWNGATE ) {
            const Castle * castle = fheroes2::getNearestCastleTownGate( *hero );
            assert( castle != nullptr );

            if ( castle->GetIndex() == hero->GetIndex() ) {
                if ( res != nullptr ) {
                    *res = _( "This hero is already in a town, so you can't cast the spell." );
                }
                return false;
            }

            const Heroes * townGuest = castle->GetHeroes().Guest();
            if ( townGuest != nullptr ) {
                if ( res != nullptr ) {
                    *res = _( "The nearest town is %{town}.\n \nThis town is occupied by your hero %{hero}." );
                    StringReplace( *res, "%{town}", castle->GetName() );
                    StringReplace( *res, "%{hero}", townGuest->GetName() );
                }
                return false;
            }
        }
    }

    return true;
}

void HeroBase::SpellCasted( const Spell & spell )
{
    magic_point -= std::min( spell.spellPoints( this ), magic_point );
    move_point -= std::min( spell.movePoints(), move_point );
}

bool HeroBase::CanLearnSpell( const Spell & spell ) const
{
    const int wisdom = GetLevelSkill( Skill::Secondary::WISDOM );

    return ( ( 4 < spell.Level() && Skill::Level::EXPERT == wisdom ) || ( 4 == spell.Level() && Skill::Level::ADVANCED <= wisdom )
             || ( 3 == spell.Level() && Skill::Level::BASIC <= wisdom ) || 3 > spell.Level() );
}

/* pack hero base */
StreamBase & operator<<( StreamBase & msg, const HeroBase & hero )
{
    return msg << static_cast<const Skill::Primary &>( hero ) << static_cast<const MapPosition &>( hero ) <<
           // modes
           hero.modes <<
           // hero base
           hero.magic_point << hero.move_point << hero.spell_book << hero.bag_artifacts;
}

/* unpack hero base */
StreamBase & operator>>( StreamBase & msg, HeroBase & hero )
{
    msg >> static_cast<Skill::Primary &>( hero ) >> static_cast<MapPosition &>( hero ) >>
        // modes
        hero.modes >> hero.magic_point >> hero.move_point >> hero.spell_book >> hero.bag_artifacts;

    return msg;
}
