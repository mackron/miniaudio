#define MAL_IMPLEMENTATION
#include "../mini_al.h"

#define DR_WAV_IMPLEMENTATION
#include "../extras/dr_wav.h"

#define JAR_MOD_IMPLEMENTATION
#include "../extras/jar_mod.h"
#undef DEBUG

#define JAR_XM_IMPLEMENTATION
#include "../extras/jar_xm.h"

#include <stdio.h>

// This is the function that's used for sending more data to the device for playback.
mal_uint32 on_send_wav_frames_to_device(mal_device* pDevice, mal_uint32 frameCount, void* pSamples)
{
	drwav* pWav = (drwav*)pDevice->pUserData;
	if (pWav == NULL) {
		return 0;
	}
	
	return (mal_uint32)drwav_read_s16(pWav, frameCount * pDevice->channels, (mal_int16*)pSamples) / pDevice->channels;
}

mal_uint32 on_send_mod_frames_to_device(mal_device* pDevice, mal_uint32 frameCount, void* pSamples)
{
	jar_mod_context_t* pMod = (jar_mod_context_t*)pDevice->pUserData;
	if (pMod == NULL) {
		return 0;
	}
	
	jar_mod_fillbuffer(pMod, (mal_int16*)pSamples, frameCount * pDevice->channels, 0);
	return (mal_uint32)(frameCount * pDevice->channels);
}

mal_uint32 on_send_xm_frames_to_device(mal_device* pDevice, mal_uint32 frameCount, void* pSamples)
{
	jar_xm_context_t* pXM = (jar_xm_context_t*)pDevice->pUserData;
	if (pXM == NULL) {
		return 0;
	}
	
	jar_xm_generate_samples_16bit(pXM, (short*)pSamples, frameCount * pDevice->channels); // (48000 / 60) / 2
	return (mal_uint32)(frameCount * pDevice->channels);
}

int main(int argc, char** argv)
{
system("pause");
	int exitcode = 0;

	if (argc < 2) {
		printf("No input file.");
		return -1;
	}

	enum { UNK, WAV, MOD, XM } type = UNK;

	jar_mod_context_t mod;
	jar_mod_init(&mod);

	jar_xm_context_t *xm = 0;

	drwav wav;
	if ( type == UNK && drwav_init_file(&wav, argv[1]) ) type = WAV;
	if ( type == UNK && (jar_xm_create_context_from_file(&xm, 48000, argv[1]) == 0)) type = XM;
	if ( type == UNK && (jar_mod_load_file(&mod, argv[1]) != 0) ) type = MOD;

	if( type == UNK ) {
		printf("Not a valid input file.");
		exitcode = -2;
		goto end;
	}

	mal_context context;
	if (mal_context_init(NULL, 0, NULL, &context) != MAL_SUCCESS) {
		printf("Failed to initialize context.");
		exitcode = -3;
		goto end;
	}

	mal_device_config config;
	if( type == WAV ) config = mal_device_config_init_playback(mal_format_s16, wav.channels, wav.sampleRate, on_send_wav_frames_to_device );
	if( type == MOD ) config = mal_device_config_init_playback(mal_format_s16, 2, mod.playrate, on_send_mod_frames_to_device );
	if( type == XM )  config = mal_device_config_init_playback(mal_format_s16, 2, 48000, on_send_xm_frames_to_device );

	mal_device device;
	if (mal_device_init(&context, mal_device_type_playback, NULL, &config, type == WAV ? (void*)&wav : type == MOD ? (void*)&mod : (void*)xm, &device) != MAL_SUCCESS) {
		printf("Failed to open playback device.");
		mal_context_uninit(&context);
		exitcode = -4;
		goto end;
	}

	mal_device_start(&device);
	
	printf("Press Enter to quit...");
	getchar();
	
	mal_device_uninit(&device);
	mal_context_uninit(&context);

	end:;
	drwav_uninit(&wav);
	jar_mod_unload(&mod);
	if(xm) jar_xm_free_context(xm);
	
	return exitcode;
}
