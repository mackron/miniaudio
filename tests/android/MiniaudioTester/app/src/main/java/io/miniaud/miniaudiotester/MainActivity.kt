package io.miniaud.miniaudiotester

import android.annotation.SuppressLint
import androidx.appcompat.app.AppCompatActivity
import android.os.Bundle
import android.widget.TextView
import io.miniaud.miniaudiotester.databinding.ActivityMainBinding

enum class AudioBackend(val value: Int)
{
    BACKEND_AUTO(0),
    BACKEND_AAUDIO(1),
    BACKEND_OPENSL(2)
}

class MainActivity : AppCompatActivity()
{
    private lateinit var binding: ActivityMainBinding

    @SuppressLint("SetTextI18n")
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)

        binding.btnPlay.setOnClickListener {
            val backend: AudioBackend;
            if (binding.radioAAudio.isChecked) {
                backend = AudioBackend.BACKEND_AAUDIO
            } else if (binding.radioOpenSL.isChecked) {
                backend = AudioBackend.BACKEND_OPENSL
            } else {
                backend = AudioBackend.BACKEND_AUTO
            }

            audioState = PlayAudio(audioState, backend.value)
            if (HasAudioError(audioState)) {
                binding.textInfo.text = GetAudioError(audioState)
            } else {
                binding.textInfo.text = "Playing..."
            }
        }

        binding.btnStop.setOnClickListener {
            audioState = PauseAudio(audioState)
            if (HasAudioError(audioState)) {
                binding.textInfo.text = GetAudioError(audioState)
            } else {
                binding.textInfo.text = "Paused."
            }
        }

        binding.btnUninit.setOnClickListener {
            audioState = UninitializeAudio(audioState)
            if (HasAudioError(audioState)) {
                binding.textInfo.text = GetAudioError(audioState)
            } else {
                binding.textInfo.text = "Device uninitialized."
            }
        }
    }

    private var audioState: Long = 0

    external fun UninitializeAudio(audioState: Long): Long
    external fun PlayAudio(audioState: Long, backend: Int): Long
    external fun PauseAudio(audioState: Long): Long
    external fun HasAudioError(audioState: Long): Boolean
    external fun GetAudioError(audioState: Long): String
    external fun DeleteAudioState(audioState: Long)

    companion object {
        // Used to load the 'miniaudiotester' library on application startup.
        init {
            System.loadLibrary("miniaudiotester")
        }
    }
}