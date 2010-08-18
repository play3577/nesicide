//    NESICIDE - an IDE for the 8-bit NES.
//    Copyright (C) 2009  Christopher S. Pow

//    This program is free software: you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation, either version 3 of the License, or
//    (at your option) any later version.

//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.

//    You should have received a copy of the GNU General Public License
//    along with this program.  If not, see <http://www.gnu.org/licenses/>.

#include "nesemulatorthread.h"

#include "dbg_cnes.h"
#include "dbg_cnesrom.h"

#include "main.h"

#undef main
#include "SDL.h"

#if defined ( IDE_BUILD )
#include "cnesicideproject.h"
#endif

#if defined ( IDE_BUILD )
extern QSemaphore breakpointSemaphore;
#endif

#include <QMutex>

SDL_AudioSpec sdlAudioSpec;
QMutex doFrameMutex;

// Hook function endpoints.
void coreMutexLock ( void )
{
   doFrameMutex.lock();
}

void coreMutexUnlock ( void )
{
   doFrameMutex.unlock();
}

void breakpointHook ( eBreakpointTarget target, eBreakpointType type, int32_t data, int32_t event )
{
   CNESDBG::CHECKBREAKPOINT(target,type,data,event);
}

extern "C" void SDL_GetMoreData(void *userdata, uint8_t *stream, int32_t len)
{
   if (nesGetAudioSamplesAvailable() < 0)
   {
      return;
   }

   SDL_MixAudio ( stream, nesGetAudioSamples(), len, SDL_MIX_MAXVOLUME );

   coreMutexLock();
   if ( emulator->isFinished() && (nesGetAudioSamplesAvailable() < APU_BUFFER_PRERENDER_THRESHOLD) )
   {
      coreMutexUnlock();
      emulator->start();
   } else
      //qDebug() << "No Render: " << apuDataAvailable;
      coreMutexUnlock();
}

NESEmulatorThread::NESEmulatorThread(QObject *)
{
   m_joy [ JOY1 ] = 0x00;
   m_joy [ JOY2 ] = 0x00;
   m_isRunning = false;
   m_isPaused = false;
   m_showOnPause = false;
   m_isStarting = false;
   m_isTerminating = false;
   m_isResetting = false;
   m_pCartridge = NULL;

   nesSetCoreMutexLockHook(coreMutexLock);
   nesSetCoreMutexUnlockHook(coreMutexUnlock);

   // Enable debugging in the external emulator library.
   nesEnableDebug ();

   // Enable breakpoint callbacks from the external emulator library.
   nesSetBreakpointHook(breakpointHook);

   SDL_Init ( SDL_INIT_AUDIO );

   coreMutexLock();
   nesClearAudioSamplesAvailable();
   coreMutexUnlock();
}

NESEmulatorThread::~NESEmulatorThread()
{

}

void NESEmulatorThread::kill()
{
#if defined ( IDE_BUILD )
   // Force hard-reset of the machine...
   CNESDBG::BREAKPOINTS(false);

   breakpointSemaphore.release();
#endif

   m_isRunning = true;
   m_isPaused = false;
   m_showOnPause = false;
   m_isTerminating = true;
}

void NESEmulatorThread::setDialog(QDialog* dialog)
{
}

#if defined ( IDE_BUILD )
void NESEmulatorThread::primeEmulator()
{
   if ( (nesicideProject) &&
        (nesicideProject->get_pointerToCartridge()) )
   {
      m_pCartridge = nesicideProject->get_pointerToCartridge();

      // Force hard-reset of the machine...
      CNESDBG::BREAKPOINTS(false);
      CNESDBG::CLEAROPCODEMASKS();

      // If we were stopped at a breakpoint, release...
      // Breakpoints have been deleted.
      if ( !(breakpointSemaphore.available()) )
      {
         breakpointSemaphore.release();
      }
   }
}
#else
void NESEmulatorThread::primeEmulator(CCartridge* pCartridge)
{
   m_pCartridge = pCartridge;
}
#endif

void NESEmulatorThread::loadCartridge()
{
   int32_t b;

   // Clear emulator's cartridge ROMs...
   nesUnloadROM();

#if defined ( IDE_BUILD )
   // Load cartridge PRG-ROM banks into emulator...
   for ( b = 0; b < m_pCartridge->getPointerToPrgRomBanks()->get_pointerToArrayOfBanks()->count(); b++ )
   {
      nesLoadPRGROMBank ( b, (uint8_t*)m_pCartridge->getPointerToPrgRomBanks()->get_pointerToArrayOfBanks()->at(b)->
                          get_pointerToBankData() );
   }

   // Load cartridge CHR-ROM banks into emulator...
   for ( b = 0; b < m_pCartridge->getPointerToChrRomBanks()->banks.count(); b++ )
   {
      nesLoadCHRROMBank ( b, (uint8_t*)m_pCartridge->getPointerToChrRomBanks()->banks.at(b)->data );
   }
#else
   // Load cartridge PRG-ROM banks into emulator...
   for ( b = 0; b < m_pCartridge->getNumPrgRomBanks(); b++ )
   {
      nesLoadPRGROMBank(b,(uint8_t*)m_pCartridge->getPointerToPrgRomBank(b));
   }

   // Load cartridge CHR-ROM banks into emulator...
   for ( b = 0; b < m_pCartridge->getNumChrRomBanks(); b++ )
   {
      nesLoadCHRROMBank(b,(uint8_t*)m_pCartridge->getPointerToChrRomBank(b));
   }
#endif

   // Perform any necessary fixup on from the ROM loading...
   nesLoadROM();

   // Set up PPU with iNES header information...
   if ( (m_pCartridge->getMirrorMode() == GameMirrorMode::NoMirroring) ||
        (m_pCartridge->getMirrorMode() == GameMirrorMode::HorizontalMirroring) )
   {
      nesSetHorizontalMirroring();
   }
   else if ( m_pCartridge->getMirrorMode() == GameMirrorMode::VerticalMirroring )
   {
      nesSetVerticalMirroring();
   }

   // CPTODO: implement mapper reloading...project reload should load ROM in saved state.
#if 0
   // Force mapper to intialize...
   mapperfunc [ m_pCartridge->getMapperNumber() ].reset ();

   MapperState* pMapperState = m_pRIID->GetMapperState ();

   if ( pMapperState->valid )
   {
      mapperfunc [ m_pRIID->GetMapperID() ].load ( pMapperState );
   }
#endif

   // Initialize NES...
   nesReset(m_pCartridge->getMapperNumber());

   emit cartridgeLoaded();
}

void NESEmulatorThread::resetEmulator()
{
   SDL_AudioSpec obtained;

#if defined ( IDE_BUILD )
   // Force hard-reset of the machine...
   CNESDBG::BREAKPOINTS(false);

   // If during the last run we were stopped at a breakpoint, clear it...
   if ( !(breakpointSemaphore.available()) )
   {
      breakpointSemaphore.release();
   }
#endif

   SDL_CloseAudio ();

   sdlAudioSpec.callback = SDL_GetMoreData;
   sdlAudioSpec.userdata = NULL;
   sdlAudioSpec.channels = 1;
   sdlAudioSpec.format = AUDIO_S16SYS;
   sdlAudioSpec.freq = 44100;

   // Set up audio sample rate for video mode...
   if ( nesGetSystemMode() == MODE_NTSC )
   {
      sdlAudioSpec.samples = APU_SAMPLES_NTSC;
   }
   else
   {
      sdlAudioSpec.samples = APU_SAMPLES_PAL;
   }

   SDL_OpenAudio ( &sdlAudioSpec, &obtained );

   SDL_PauseAudio ( 0 );

   m_isResetting = true;
}

void NESEmulatorThread::startEmulation ()
{
#if defined ( IDE_BUILD )
   // If during the last run we were stopped at a breakpoint, clear it...
   if ( !(breakpointSemaphore.available()) )
   {
      breakpointSemaphore.release();
   }
#endif

   m_isStarting = true;
}

#if defined ( IDE_BUILD )
void NESEmulatorThread::stepCPUEmulation ()
{
   // If during the last run we were stopped at a breakpoint, clear it...
   // But ensure we come right back...
   CNESDBG::STEPCPUBREAKPOINT();
   if ( !(breakpointSemaphore.available()) )
   {
      breakpointSemaphore.release();
   }

   m_isStarting = true;
}

void NESEmulatorThread::stepPPUEmulation ()
{
   // If during the last run we were stopped at a breakpoint, clear it...
   // But ensure we come right back...
   CNESDBG::STEPPPUBREAKPOINT();
   if ( !(breakpointSemaphore.available()) )
   {
      breakpointSemaphore.release();
   }

   m_isStarting = true;
}
#endif

void NESEmulatorThread::pauseEmulation (bool show)
{
   m_isStarting = false;
   m_isRunning = false;
   m_isPaused = true;
   m_showOnPause = show;
}

void NESEmulatorThread::run ()
{
   while ( m_isStarting || m_isRunning || m_isResetting )
   {

      // Allow thread exit...
      if ( m_isTerminating )
      {
         break;
      }

      // Allow thread to keep going...
      if ( m_isStarting )
      {
         m_isStarting = false;
         m_isRunning = true;
         m_isPaused = false;

#if defined ( IDE_BUILD )
         // Trigger Breakpoint dialog redraw...
         emit breakpointClear();
#endif

         // Trigger emulator UI button update...
         emit emulatorStarted();
      }

      // Properly coordinate NES reset with emulator...
      if ( m_isResetting )
      {
         // Reset emulated I/O devices...
         m_joy [ JOY1 ] = 0x00;
         m_joy [ JOY2 ] = 0x00;

#if defined ( IDE_BUILD )
         // Re-enable breakpoints that were previously enabled...
         CNESDBG::BREAKPOINTS(true);
#endif

         // Reset NES...
         nesReset();

         // Allow cartridge to be loaded...
         if ( m_pCartridge )
         {
            // Reload the cartridge image...
            // This internally causes a NES reset.
            loadCartridge();
            m_pCartridge = NULL;
         }

         emit emulatorReset();

         // Don't *keep* resetting...
         m_isResetting = false;
      }

      // Run the NES...
      if ( m_isRunning )
      {
#if defined ( IDE_BUILD )
         // Make sure breakpoint semaphore is on the precipice...
         breakpointSemaphore.tryAcquire();
#endif
         coreMutexLock();
         if (nesGetAudioSamplesAvailable() >= APU_BUFFER_PRERENDER)
         {
             coreMutexUnlock();
             break;
         }
         coreMutexUnlock();
         // Run emulator for one frame...
         SDL_LockAudio();
         nesRun(m_joy);
         SDL_UnlockAudio();

         emit emulatedFrame();
      }

      // Pause?
      if ( m_isPaused )
      {
         // Trigger inspectors to update on a pause also...
         emit emulatorPaused(m_showOnPause);
         m_isPaused = false;
         m_isRunning = false;

//         CAPU::PAUSE();
      }

   }

   return;
}