using NAudio.Wave;
using Vosk;
using System.Threading;
using System;

namespace DSN
{
    class VoskRecognizer {
        private const int TRAIN_TIMEOUT = 120000; // 2min timout for voice2json training

        public delegate void SpeechRecognizedHandler(string resultJson);
        public event SpeechRecognizedHandler SpeechRecognized;

        private Configuration config;
        private WaveInEvent mic;
        private Model model;
        private VoskRecognizer recognizer;

        public VoskRecognizer(Configuration config) {
            this.config = config;
            init();
        }

        public void StopRecording() {
            if (mic != null) {
                mic.StopRecording();
                mic.Dispose();
                mic = null;
            }
        }

        public void SetInputToDefaultAudioDevice() {
            StopRecording();
            // voice2json expects 16-bit 16Khz mono audio as input
            mic = new WaveInEvent { WaveFormat = new WaveFormat(16000, 16, 1) };
            this.mic.DataAvailable += this.WaveSourceDataAvailable;
            mic.StartRecording();
        }

        private void init() {
            string model = AppDomain.CurrentDomain.BaseDirectory + "\\" + "VoskModels\\" + config.GetLocale();

        }

        public void LoadJSGF(string jsgf) {

        }

        public void RecognizeAsyncCancel() {
        }

        public void RecognizeAsync() {

        }

        private void WaveSourceDataAvailable(object sender, WaveInEventArgs e) {
            //Trace.TraceInformation("WaveSourceDataAvailable: {0}, {1}", sessionId, e.BytesRecorded);
        }
    }
}
