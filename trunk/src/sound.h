//
//  sound
//
// requires libs:  dxguid.lib dsound.lib
//

#ifndef SM_SOUND_H
#define SM_SOUND_H

#ifdef NOSOUND
class Sound;
inline int    SoundInit(){};
inline void   SoundClose(){};
inline Sound* SoundCreate(const char *WaveFileName, int num_buffers=1){return NULL;}
inline int    SoundPlay(Sound *sound, int loop_it=0 ,unsigned int dwPriority=0,  long lVolume=0, long lFrequency=-1 ){return 0;}
inline void   SoundStop(Sound *sound){;}
inline void   SoundReset(Sound *sound){;}
inline int    SoundIsPlaying(Sound *sound){;}


#else

//struct FMOD_SOUND;
//typedef struct FMOD_SOUND Sound;

class Sound;

int    SoundInit();
void   SoundClose();
Sound* SoundCreate(const char *WaveFileName, int num_buffers=1);
int    SoundPlay(Sound *sound, int loop_it=0 ,unsigned int dwPriority=0,  long lVolume=0, long lFrequency=-1 );
void   SoundStop(Sound *sound);
void   SoundReset(Sound *sound);
int    SoundIsPlaying(Sound *sound);

#endif  // NOSOUND

#endif  // SM_SOUND_H
