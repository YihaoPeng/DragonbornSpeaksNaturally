using System;
using System.Collections.Generic;
using System.Linq;
using System.Speech.Recognition;
using System.Diagnostics;
using System.Globalization;
using System.Threading;
using NAudio.CoreAudioApi;
using Newtonsoft.Json.Linq;

namespace DSN {
    class Voice2JsonSpeechRecognition : NAudio.CoreAudioApi.Interfaces.IMMNotificationClient, ISpeechRecognitionManager {
        private const long STATUS_STOPPED = 0; // not in recognizing
        private const long STATUS_RECOGNIZING = 1; // in recognizing
        private const long STATUS_WAITING_DEVICE = 2; // waiting for record device

        private const string DEFAULT_PAUSE_AUDIO_FILE = @"C:\Windows\media\Speech Off.wav";
        private const string DEFAULT_RESUME_AUDIO_FILE = @"C:\Windows\media\Speech On.wav";

        public event DialogueLineRecognitionHandler OnDialogueLineRecognized;

        private bool isPaused = false;
        private List<RecognitionGrammar> pausePhrases = new List<RecognitionGrammar>();
        private List<RecognitionGrammar> resumePhrases = new List<RecognitionGrammar>();
        private readonly string pauseAudioFile;
        private readonly string resumeAudioFile;

        private long recognitionStatus = STATUS_WAITING_DEVICE; // Need thread safety.
        private Thread waitingDeviceThread;

        private readonly Voice2JsonCli DSN;
        private readonly Object dsnLock = new Object();

        // Dialogue can be more generous in the min confidence because
        // phrases are usually longer and more distinct amongst themselves
        private readonly float dialogueMinimumConfidence = 0.5f;
        private readonly float commandMinimumConfidence = 0.7f;
        private readonly bool logAudioSignalIssues = false;

        private bool isDialogueMode = false;
        private ISpeechRecognitionGrammarProvider[] grammarProviders;

        private string currentDeviceId = null;
        private readonly MMDeviceEnumerator deviceEnum = new MMDeviceEnumerator();
        private readonly Configuration config;

        private long sessionId = 0;
        private List<RecognitionGrammar> allGrammars;

        public Voice2JsonSpeechRecognition(Configuration config) {
            this.config = config;

            dialogueMinimumConfidence = float.Parse(config.Get("SpeechRecognition", "dialogueMinConfidence", "0.5"), CultureInfo.InvariantCulture);
            commandMinimumConfidence = float.Parse(config.Get("SpeechRecognition", "commandMinConfidence", "0.7"), CultureInfo.InvariantCulture);
            logAudioSignalIssues = config.Get("SpeechRecognition", "bLogAudioSignalIssues", "0") == "1";

            Trace.TraceInformation("Speech Recognition Engine: Voice2Json (stub)");
            Trace.TraceInformation("Locale: {0}\nDialogueConfidence: {1}\nCommandConfidence: {2}", config.GetLocale(), dialogueMinimumConfidence, commandMinimumConfidence);

            pauseAudioFile = config.Get("SpeechRecognition", "pauseAudioFile", DEFAULT_PAUSE_AUDIO_FILE).Trim();
            resumeAudioFile = config.Get("SpeechRecognition", "resumeAudioFile", DEFAULT_RESUME_AUDIO_FILE).Trim();

            List<string> pausePhraseStrings = config.GetPausePhrases();
            List<string> resumePhraseStrings = config.GetResumePhrases();
            foreach (string phrase in pausePhraseStrings) {
                if (phrase == null || phrase.Trim() == "")
                    continue;
                Trace.TraceInformation("Found pause phrase: '{0}'", phrase);
                try {
                    RecognitionGrammar g = Phrases.createGrammar(Phrases.normalize(phrase, config), config);
                    pausePhrases.Add(g);
                } catch (Exception ex) {
                    Trace.TraceError("Failed to create grammar for pause phrase {0} due to exception:\n{1}", phrase, ex.ToString());
                }
            }
            foreach (string phrase in resumePhraseStrings) {
                if (phrase == null || phrase.Trim() == "")
                    continue;
                Trace.TraceInformation("Found resume phrase: '{0}'", phrase);
                try {
                    RecognitionGrammar g = Phrases.createGrammar(Phrases.normalize(phrase, config), config);
                    resumePhrases.Add(g);
                } catch (Exception ex) {
                    Trace.TraceError("Failed to create grammar for resume phrase {0} due to exception:\n{1}", phrase, ex.ToString());
                }
            }

            this.DSN = new Voice2JsonCli(config);
            this.DSN.SpeechRecognized += DSN_SpeechRecognized;
            this.deviceEnum.RegisterEndpointNotificationCallback(this);

            WaitRecordingDeviceNonBlocking();
        }

        private void WaitRecordingDeviceNonBlocking() {
            // Waiting recording device in a new thread to avoid blocking
            waitingDeviceThread = new Thread(DoWaitRecordingDevice);
            waitingDeviceThread.Start();
        }

        private void DoWaitRecordingDevice() {
            lock (dsnLock) {
                StopRecognition();
                recognitionStatus = STATUS_WAITING_DEVICE;
            }

            // Select input device
            try {
                this.DSN.SetInputToDefaultAudioDevice();
                Trace.TraceInformation("Recording device is ready.");
            } catch {
                Trace.TraceInformation("Waiting for recording device...");
                while (config.IsRunning()) {
                    try {
                        this.DSN.SetInputToDefaultAudioDevice();
                        Trace.TraceInformation("Recording device is ready.");
                        break;
                    } catch {
                        Thread.Sleep(500);
                    }
                }
            }

            lock (dsnLock) {
                if (!config.IsRunning()) {
                    return;
                }

                MMDevice device = deviceEnum.GetDefaultAudioEndpoint(DataFlow.Capture, Role.Console);
                if (device != null) {
                    currentDeviceId = device.ID;
                }

                recognitionStatus = STATUS_STOPPED;
                RestartRecognition();
                waitingDeviceThread = null;
            }
        }

        public void Stop() {
            config.Stop();
            DSN.StopRecording();
            StopRecognition();

            lock (dsnLock) {
                deviceEnum.UnregisterEndpointNotificationCallback(this);

                if (waitingDeviceThread != null) {
                    waitingDeviceThread.Abort();
                }
            }
        }

        private void RestartRecognition() {
            StartSpeechRecognition(isDialogueMode, grammarProviders);
        }

        private void DSN_AudioStateChanged(object sender, AudioStateChangedEventArgs e) {
            if (logAudioSignalIssues) {
                Trace.TraceInformation("Audio state changed: {0}", e.AudioState.ToString());
            }
        }

        private void DSN_AudioSignalProblemOccurred(object sender, AudioSignalProblemOccurredEventArgs e) {
            if (logAudioSignalIssues) {
                Trace.TraceInformation("Audio signal problem occurred during speech recognition: {0}", e.AudioSignalProblem.ToString());
            }
        }

        public void StopRecognition() {
            lock (dsnLock) {
                try {
                    if (recognitionStatus == STATUS_RECOGNIZING) {
                        this.DSN.RecognizeAsyncCancel();
                        recognitionStatus = STATUS_STOPPED;
                    }
                } catch (Exception e) {
                    Trace.TraceError("Failed to stop recognition due to exception");
                    Trace.TraceError(e.ToString());
                }
            }
        }

        public void StartSpeechRecognition(bool isDialogueMode, params ISpeechRecognitionGrammarProvider[] grammarProviders) {
            lock (dsnLock) {
                try {
                    this.isDialogueMode = isDialogueMode;
                    this.grammarProviders = grammarProviders;

                    if (recognitionStatus == STATUS_WAITING_DEVICE) {
                        // Avoid blocking and the program cannot quit when Skyrim is terminated
                        Trace.TraceInformation("Recording device is not ready");
                        return;
                    }

                    StopRecognition(); // Cancel previous recognition

                    allGrammars = new List<RecognitionGrammar>();
                    if (isPaused) {
                        allGrammars.AddRange(resumePhrases);
                    } else {
                        if (grammarProviders != null) {
                            allGrammars.AddRange(grammarProviders.SelectMany((x) => x.GetGrammars()).ToList());
                        }
                        allGrammars.AddRange(pausePhrases);
                    }
                    // Error is thrown if no grammars are loaded
                    if (allGrammars.Count > 0) {
                        SetGrammar(allGrammars);
                        this.DSN.RecognizeAsync();
                        recognitionStatus = STATUS_RECOGNIZING;

                        Trace.TraceInformation(
                            "Recognition {0}: {1} mode, {2} phrases",
                            isPaused ? "paused" : "started",
                            isDialogueMode ? "dialogue" : "command",
                            allGrammars.Count
                        );
                    }
                } catch (Exception e) {
                    Trace.TraceError("Failed to start new phrase recognition due to exception");
                    Trace.TraceError(e.ToString());
                }
            }
        }

        private void SetGrammar(List<RecognitionGrammar> grammars) {
            var id = Interlocked.Increment(ref sessionId);
            string jsgf = "";
            int i = 0;
            foreach (RecognitionGrammar grammar in grammars) {
                try {
                    jsgf += GrammarToJSGF(grammar, id, i);
                } catch (Exception ex) {
                    Trace.TraceError("Load grammar '{0}' failed:\n{1}", grammar.Name, ex.ToString());
                }
                i++;
            }

            //Trace.TraceInformation("JSGF:\n{0}", jsgf);
            this.DSN.LoadJSGF(jsgf);
        }

        private string GrammarToJSGF(RecognitionGrammar grammar, long sessionId, int index) {
            string jsgf = "[dsn_" + sessionId + "_" + index + "]\n" + grammar.ToJSGF() + "\n\n";
            return jsgf;
        }

        private void DSN_SpeechRecognized(string resultJson) {
            /*
             * JSON with some recognized:
             {
	            "text": "装备 黑 檀 弓",
	            "likelihood": 0.05448459450513476,
	            "transcribe_seconds": 8.321307364999939,
	            "wav_seconds": 0.0163125,
	            "tokens": ["装备", "黑", "檀", "弓"],
	            "timeout": false,
	            "intent": {
		            "name": "dsn_28",
		            "confidence": 0.75
	            },
	            "entities": [],
	            "raw_text": " 装备 黑 檀 弓 弓",
	            "recognize_seconds": 0.001171658999965075,
	            "raw_tokens": ["装备", "黑", "檀", "弓"],
	            "speech_confidence": null,
	            "wav_name": null,
	            "slots": {}
            }
            * JSON with nothing recognized:
            {
	            "text": "",
	            "likelihood": 0.5669604241019671,
	            "transcribe_seconds": 6.2813740700000835,
	            "wav_seconds": 0.01225,
	            "tokens": [],
	            "timeout": false,
	            "intent": {
		            "name": "",
		            "confidence": 0
	            },
	            "entities": [],
	            "raw_text": "",
	            "recognize_seconds": 0,
	            "raw_tokens": [],
	            "speech_confidence": null,
	            "wav_name": null,
	            "slots": {}
            }
            */
            dynamic result = JObject.Parse(resultJson);
            if (result.text == null || result.text == "") {
                return;
            }
            string text = result.text;
            string intent = result.intent.name;
            float confidence = result.intent.confidence;

            if (logAudioSignalIssues) {
                Trace.TraceInformation("Recognition log: '{0}' (Confidence: {1})", text, confidence);
            }

            var intentParts = intent.Split('_');
            if (intentParts.Length != 3 || intentParts[0] != "dsn") {
                return;
            }

            var resultSessionId = Convert.ToInt64(intentParts[1]);
            if (Interlocked.Read(ref sessionId) != resultSessionId) {
                return;
            }

            int grammarIndex = Convert.ToInt32(intentParts[2]);
            var grammar = allGrammars[grammarIndex];

            lock (dsnLock) {
                if (pausePhrases.Contains(grammar) || resumePhrases.Contains(grammar)) {
                    if (confidence >= commandMinimumConfidence) {
                        StopRecognition();

                        isPaused = !isPaused;
                        Trace.TraceInformation("****** Recognition {0} ******", isPaused ? "Paused" : "Resumed");

                        // Play a tone for notification
                        string file = isPaused ? pauseAudioFile : resumeAudioFile;
                        if (file.Count() != 0) {
                            try {
                                new System.Media.SoundPlayer(file).Play();
                            } catch (Exception ex) {
                                Trace.TraceError("Play {0} failed with exception:\n{1}", file, ex.ToString());
                            }
                        }

                        RestartRecognition();
                    } else {
                        Trace.TraceInformation("Recognized phrase '{0}' but ignored because confidence was too low (Confidence: {1})", text, confidence);
                    }
                    return;
                }

                float minConfidence = isDialogueMode ? dialogueMinimumConfidence : commandMinimumConfidence;
                if (confidence >= minConfidence) {
                    Trace.TraceInformation("Recognized phrase '{0}' (Confidence: {1})", text, confidence);
                    OnDialogueLineRecognized?.Invoke(text, grammar, "");
                } else {
                    Trace.TraceInformation("Recognized phrase '{0}' but ignored because confidence was too low (Confidence: {1})", text, confidence);
                }
            }
        }

        public void OnDefaultDeviceChangedThread(DataFlow dataFlow, Role deviceRole, string defaultDeviceId) {
            lock (dsnLock) {
                if (defaultDeviceId == null && currentDeviceId != null) {
                    Trace.TraceInformation("****** Default audio device not available ******");
                    currentDeviceId = null;
                    return;
                }
                if (defaultDeviceId != currentDeviceId) {
                    Trace.TraceInformation("****** Default audio device changed ******");
                    currentDeviceId = defaultDeviceId;
                    WaitRecordingDeviceNonBlocking();
                }
            }
        }

        // IMMNotificationClient events
        public void OnDefaultDeviceChanged(DataFlow dataFlow, Role deviceRole, string defaultDeviceId) {
            if (dataFlow != DataFlow.Capture || deviceRole != Role.Console) {
                return;
            }
            // A deadlock will occur if you call `lock (dsnLock)` in this thread.
            new Thread(() => OnDefaultDeviceChangedThread(dataFlow, deviceRole, defaultDeviceId)).Start();
        }
        public void OnDeviceAdded(string deviceId) {
        }
        public void OnDeviceRemoved(string deviceId) {
        }
        public void OnDeviceStateChanged(string deviceId, DeviceState newState) {
        }
        public void OnPropertyValueChanged(string deviceId, PropertyKey propertyKey) {
        }
    }
}
