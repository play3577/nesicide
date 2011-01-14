// BasePalette.h: interface for the CBasePalette class.
//
//////////////////////////////////////////////////////////////////////

#if !defined ( BASE_PALETTE_H )
#define BASE_PALETTE_H

#include "emulator_core.h"

#include <QColor>

class CBasePalette
{
public:
   static inline QColor& GetPalette ( int idx, int bMonochrome = 0, int bEmphasizeRed = 0, int bEmphasizeGreen = 0, int bEmphasizeBlue = 0 )
   {
      if ( bMonochrome )
      {
         idx&= 0xF0;
      }

      return *(*(m_paletteVariants+((bEmphasizeRed)|((bEmphasizeGreen)<<1)|((bEmphasizeBlue)<<2)))+idx);
   }
   static inline int8_t GetPaletteR ( int idx, int bMonochrome = 0, int bEmphasizeRed = 0, int bEmphasizeGreen = 0, int bEmphasizeBlue = 0 )
   {
      if ( bMonochrome )
      {
         idx&= 0xF0;
      }

      return *(*(*(m_paletteRGBs+((bEmphasizeRed)|((bEmphasizeGreen)<<1)|((bEmphasizeBlue)<<2)))+idx));
   }
   static inline int8_t GetPaletteG ( int idx, int bMonochrome = 0, int bEmphasizeRed = 0, int bEmphasizeGreen = 0, int bEmphasizeBlue = 0 )
   {
      if ( bMonochrome )
      {
         idx&= 0xF0;
      }

      return *(*(*(m_paletteRGBs+((bEmphasizeRed)|((bEmphasizeGreen)<<1)|((bEmphasizeBlue)<<2)))+idx)+1);
   }
   static inline int8_t GetPaletteB ( int idx, int bMonochrome = 0, int bEmphasizeRed = 0, int bEmphasizeGreen = 0, int bEmphasizeBlue = 0 )
   {
      if ( bMonochrome )
      {
         idx&= 0xF0;
      }

      return *(*(*(m_paletteRGBs+((bEmphasizeRed)|((bEmphasizeGreen)<<1)|((bEmphasizeBlue)<<2)))+idx)+2);
   }

   static inline void SetPalette ( int idx, QColor& color )
   {
      m_paletteVariants[0][idx] = color;
      CalculateVariants ();
   }
   static void CalculateVariants ( void );
   static void RestoreBase ( void )
   {
      memcpy ( m_paletteVariants[0], m_paletteBase, sizeof(m_paletteBase) );
      CalculateVariants ();
   }
   CBasePalette()
   {
      memcpy ( m_paletteVariants[0], m_paletteBase, sizeof(m_paletteBase) );
      CalculateVariants ();
   };

protected:
   static QColor m_paletteBase [ 64 ];
   static QColor m_paletteVariants [ 8 ] [ 64 ];
   static int8_t   m_paletteRGBs [ 8 ] [ 64 ] [ 3 ];
};

#endif
