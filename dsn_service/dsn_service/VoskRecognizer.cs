using NAudio.Wave;
using Vosk;
using System;

namespace DSN
{
    class VoskRecognizer {
        public delegate void SpeechRecognizedHandler(string resultJson);
        public event SpeechRecognizedHandler SpeechRecognized;

        private Configuration config;
        private WaveInEvent mic;
        private Model model;
        private Vosk.VoskRecognizer recognizer;
        private string grammar;

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
            // vosk expects 16-bit 16Khz mono audio as input
            mic = new WaveInEvent { WaveFormat = new WaveFormat(16000, 16, 1) };
            this.mic.DataAvailable += this.WaveSourceDataAvailable;
            mic.StartRecording();
        }

        private void init() {
            string modelDir = AppDomain.CurrentDomain.BaseDirectory + "\\" + "VoskModels\\" + config.GetLocale();
            model = new Model(modelDir);
        }

        public void LoadGrammar(string grammar) {
            this.grammar = grammar;
        }

        public void RecognizeAsyncCancel() {
            recognizer = null;
        }

        public void RecognizeAsync() {
            recognizer = new Vosk.VoskRecognizer(model, 16000.0f, grammar);
        }

        private void WaveSourceDataAvailable(object sender, WaveInEventArgs e) {
            //Trace.TraceInformation("WaveSourceDataAvailable: {0}, {1}", sessionId, e.BytesRecorded);

            if (recognizer == null) {
                return;
            }

            recognizer.AcceptWaveform(e.Buffer, e.BytesRecorded);
        }
    }
}
