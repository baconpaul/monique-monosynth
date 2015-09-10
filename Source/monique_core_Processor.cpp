
#include "monique_core_Processor.h"
#include "monique_core_Datastructures.h"
#include "monique_core_Synth.h"

#include "monique_ui_MainWindow.h"
#include "monique_ui_SegmentedMeter.h"


//==============================================================================
//==============================================================================
//==============================================================================
template<typename T, int size>
class CircularBuffer 
{
    int pos;
    T buffer[size];
    T sum;

public:
    inline void add( T val_ ) noexcept {
        sum-=buffer[pos];
        buffer[pos] = val_;
        sum+=val_;

        ++pos;
        if( pos == size )
            pos = 0;
    }
    inline T get_average() const noexcept {
        return sum / size;
    }

    CircularBuffer() : pos(0), sum(0) {
        for( int i = 0 ; i != size ; ++i )
            buffer[i] = 0;
    }
};

//==============================================================================
//==============================================================================
//==============================================================================
MoniqueAudioProcessor::MoniqueAudioProcessor()
    :
#ifdef IS_STANDALONE
    clock_smoth_buffer( new type_CLOCK_SMOTH_BUFFER ),
#endif

    peak_meter(nullptr),
    repaint_peak_meter(false)
{
    // CREATE SINGLETONS
    RuntimeNotifyer::getInstance();

    std::cout << "MONIQUE: init processor" << std::endl;
    FloatVectorOperations::enableFlushToZeroMode(true);
    {
        AppInstanceStore::getInstance()->audio_processor = this;

        std::cout << "MONIQUE: init core" << std::endl;
        synth = new Synthesiser();
        synth_data = new MoniqueSynthData(MASTER);

#ifdef IS_PLUGIN
        voice = new MoniqueSynthesiserVoice(this,synth_data);
        synth->addVoice (voice);
        synth->addSound (new MoniqueSynthesiserSound());
#else // IS_STANDALONE
        {
            voice = new MoniqueSynthesiserVoice(this,synth_data);
            synth->addVoice (voice);
            synth->addSound (new MoniqueSynthesiserSound());
        }

        audio_is_successful_initalized = (mono_AudioDeviceManager::read() == "");
        if( audio_is_successful_initalized )
        {
            setPlayConfigDetails ( 0, 2, 0, 0);
            addAudioCallback (&player);
            player.setProcessor (this);
        }
#endif
    }

    std::cout << "MONIQUE: init midi" << std::endl;
    {
        synth_data->load_settings();
        synth_data->read_midi();
#ifdef IS_STANDALONE
        synth_data->load();
#endif
        MidiKeyboardState::addListener(this);
    }
}

// ----------------------------------------------------
MoniqueAudioProcessor::~MoniqueAudioProcessor()
{
    // trigger_send_clear_feedback();
    // stop_midi_devices();
#ifdef IS_STANDALONE
    mono_AudioDeviceManager::save();
    closeAudioDevice();
    removeAudioCallback (&player);
    player.setProcessor(nullptr);
#endif
    synth_data->save_midi();
    synth_data->save_settings();

    AppInstanceStore::getInstance()->audio_processor = nullptr;

    delete synth;
    delete synth_data;
}

//==============================================================================
/*
#ifdef IS_STANDALONE
void MoniqueAudioProcessor::handle_extern_midi_start( const MidiMessage& message ) noexcept
{
    data_in_processor->messageCollector.addMessageToQueue( message );
}
void MoniqueAudioProcessor::handle_extern_midi_stop( const MidiMessage& message) noexcept
{

    data_in_processor->messageCollector.addMessageToQueue( message );

}
void MoniqueAudioProcessor::handle_extern_midi_continue( const MidiMessage& message ) noexcept
{

}
void MoniqueAudioProcessor::handle_extern_midi_clock( const MidiMessage& message ) noexcept
{
    if( synth_data->sync ) {
        clock_smoth_buffer->add( message.getTimeStamp()-last_clock_sample );
        last_clock_sample = message.getTimeStamp();

        data_in_processor->messageCollector.addMessageToQueue( message );
    }
}
#endif
*/

//==============================================================================
//==============================================================================
//==============================================================================
/*
void MoniqueAudioProcessor::handle_extern_note_input( const MidiMessage& message ) noexcept
{
    //MidiKeyboardState::processNextMidiEvent( message );
    //MidiBuffer buffer( message );
    // data_in_processor->handle_note_input( buffer );

    // TODO, TR SENDS NOTES
    data_in_processor->messageCollector.addMessageToQueue( message );
}
void MoniqueAudioProcessor::handle_extern_cc_input( const MidiMessage& message_ ) noexcept
{
    MidiBuffer buffer( message_ );
    data_in_processor->handle_cc_input( buffer );
}
void MoniqueAudioProcessor::trigger_send_feedback() noexcept
{
    Array< Parameter* >& parameters = synth_data->get_atomateable_parameters();
    for( int i = 0 ; i != parameters.size() ; ++ i )
        parameters.getUnchecked(i)->midi_control->send_feedback_only();
}
void MoniqueAudioProcessor::trigger_send_clear_feedback() noexcept
{
    Array< Parameter* >& parameters = synth_data->get_atomateable_parameters();
    for( int i = 0 ; i != parameters.size() ; ++ i )
        parameters.getUnchecked(i)->midi_control->send_clear_feedback_only();
}
*/
//==============================================================================
//==============================================================================
//==============================================================================
void MoniqueAudioProcessor::processBlock ( AudioSampleBuffer& buffer_, MidiBuffer& midi_messages_ )
{

    if( sample_rate != getSampleRate() || getBlockSize() != block_size )
    {
        prepareToPlay(getSampleRate(),getBlockSize());
    }

    const int num_samples = buffer_.getNumSamples();
    buffer_.clear();

#ifdef IS_STANDALONE
    static bool is_first_time = true;
    if(is_first_time)
    {
        current_pos_info.resetToDefault();
        is_first_time=false;
    }
    current_pos_info.bpm = synth_data->speed;
    current_pos_info.timeSigNumerator = 4;
    current_pos_info.timeSigDenominator = 4;
    current_pos_info.isPlaying = true;
    current_pos_info.isRecording = false;
    current_pos_info.timeInSamples += buffer_.getNumSamples();
    {
        {
#else // PLUGIN
    if ( getPlayHead() != 0 )
    {
        if( getPlayHead()->getCurrentPosition ( current_pos_info ) )
        {
#endif
            if( current_pos_info.timeInSamples + num_samples >= 0 && current_pos_info.isPlaying )
            {
                //data_in_processor->incomingMidi.clear();
                //data_in_processor->messageCollector.removeNextBlockOfMessages (data_in_processor->incomingMidi, num_samples);
#ifndef IS_STANDALONE
                MidiBuffer::Iterator message_iter( data_in_processor->incomingMidi );
                MidiMessage extern_midi_message;
                int sample_position;
                RuntimeInfo& runtime_info = GET_DATA( runtime_info );
                runtime_info.next_step_at_sample.clearQuick();
                runtime_info.next_step.clearQuick();
                while( message_iter.getNextEvent( extern_midi_message, sample_position ) )
                {
                    int64 abs_event_time_in_samples = current_pos_info.timeInSamples+sample_position;
                    if( extern_midi_message.isNoteOnOrOff() )
                    {
                        midi_messages_.addEvent( extern_midi_message, sample_position );
                    }
                    else if( extern_midi_message.isMidiStart() )
                    {
                        last_start_sample = abs_event_time_in_samples;
                        runtime_info.clock_counter = 0;
                        runtime_info.is_extern_synced = true;
                        runtime_info.is_running = true;
                    }
                    else if( extern_midi_message.isMidiStop() )
                    {
                        MidiMessage notes_off = MidiMessage::allNotesOff(1);
                        midi_messages_.addEvent( notes_off, sample_position );
                        runtime_info.is_extern_synced = false;
                        runtime_info.is_running = false;
                    }
                    else if( extern_midi_message.isMidiClock() )
                    {
                        // TODO  der sync muss immer gesteted werden!
                        if( synth_data->sync )
                        {
                            synth_data->speed = 60.0/(clock_smoth_buffer->get_average() * 24);
                        }

                        //todo clean up
                        //todo not needed in plugin
                        int factor = 96/16;
                        static int samples_to_count = 96;
                        /*
                                    switch( synth_data->arp_sequencer_data->speed_multi ) {
                                    case _XNORM :
                                        factor = 96/16;
                                        break;
                                    case _X2 :
                                        factor = 96/32;
                                        break;
                                    case _X05 :
                                        factor = 96/8;
                                        break;
                                    case _X4 :
                                        factor = 96/64; // BUG
                                        break;
                                    case _X025 :
                                        factor = 96/4;
                                        break;
                                    default  : //TODO
                                        factor = 96/48;
                                        break;
                                    }
                                    */
                        if( runtime_info.clock_counter%factor == 0 )
                        {
                            if( runtime_info.is_running )
                            {
                                runtime_info.next_step_at_sample.add( sample_position );
                                runtime_info.next_step.add( runtime_info.clock_counter/factor );
                            }
                        }
                        runtime_info.clock_counter++;
                        if( runtime_info.clock_counter == samples_to_count )
                        {
                            runtime_info.clock_counter = 0;

                            samples_to_count = 96;
                            /*
                                        switch( synth_data->arp_sequencer_data->speed_multi )
                                        {
                                        case _XNORM :
                                            samples_to_count = 96;
                                            break;
                                        case _X2 :
                                            samples_to_count = 96/2;
                                            break;
                                        case _X05 :
                                            samples_to_count = 96*2;
                                            break;
                                        case _X4 :
                                            samples_to_count = 96/4;
                                            break;
                                        case _X025 :
                                            samples_to_count = 96*4;
                                            break;
                                        default   : //TODO
                                            samples_to_count = 96/3;
                                            break;
                                        }
                                        */
                        }
                    }
                }
#endif
#ifdef PROFILE
                {
                    static MidiMessage note_on( MidiMessage::noteOn( 1, 64, 1.0f ) );
                    static bool is_first_block = true;
                    if( is_first_block )
                    {
                        is_first_block = false;
                        midi_messages_.addEvent( note_on, 0 );
                    }

                    if( current_pos_info.timeInSamples > 44100 * 10 )
                        exit(0);
                    else
                        std::cout << "PROCESS NUM SAMPLES:" << current_pos_info.timeInSamples << " LEFT:" << 44100 * 10 - current_pos_info.timeInSamples << std::endl;
                }
#endif
                // MIDI IN
                AppInstanceStore::getInstance()->lock_amp_painter();
                {
                    // RENDER SYNTH
                    get_cc_input_messages( midi_messages_, num_samples );
                    get_note_input_messages( midi_messages_, num_samples );
                    synth->renderNextBlock ( buffer_, midi_messages_, 0, num_samples );
                    midi_messages_.clear(); // WILL BE FILLED AT THE END

                    // VISUALIZE
                    if( peak_meter )
                        peak_meter->process( buffer_.getReadPointer(0), num_samples );
                }
                AppInstanceStore::getInstance()->unlock_amp_painter();
            }
        }
    }
}

//==============================================================================
//==============================================================================
//==============================================================================
void MoniqueAudioProcessor::prepareToPlay ( double sampleRate, int block_size_ )
{
    // TODO optimize functions without sample rate and block size
    // TODO replace audio sample buffer??
    GET_DATA(data_buffer).resize_buffer_if_required(block_size_);
    synth->setCurrentPlaybackSampleRate (sampleRate);
    RuntimeNotifyer::getInstance()->set_sample_rate( sampleRate );
    RuntimeNotifyer::getInstance()->set_block_size( block_size_ );
    voice->reset_internal();
}
void MoniqueAudioProcessor::releaseResources()
{
    // TODO reset all
}
void MoniqueAudioProcessor::reset()
{
}

//==============================================================================
//==============================================================================
//==============================================================================
int MoniqueAudioProcessor::getNumParameters()
{
    return synth_data->get_atomateable_parameters().size();
}
bool MoniqueAudioProcessor::isParameterAutomatable ( int ) const
{
    return true;
}
float MoniqueAudioProcessor::getParameter( int i_ )
{
    return get_percent_value( synth_data->get_atomateable_parameters().getUnchecked(i_) );
}
const String MoniqueAudioProcessor::getParameterText( int i_ )
{
    return String( get_percent_value( synth_data->get_atomateable_parameters().getUnchecked(i_) ) );
}
String MoniqueAudioProcessor::getParameterLabel (int i_) const {
    return  "N/A";
}
int MoniqueAudioProcessor::getParameterNumSteps( int i_ )
{
    return synth_data->get_atomateable_parameters().getUnchecked(i_)->get_info().num_steps;
}
float MoniqueAudioProcessor::getParameterDefaultValue( int i_ )
{
    return get_percent_default_value( synth_data->get_atomateable_parameters().getUnchecked(i_) );
}
const String MoniqueAudioProcessor::getParameterName( int i_ )
{
    return synth_data->get_atomateable_parameters().getUnchecked(i_)->get_info().short_name;
}
void MoniqueAudioProcessor::setParameter( int i_, float percent_ )
{
    set_percent_value( synth_data->get_atomateable_parameters().getUnchecked(i_), percent_ );
}

//==============================================================================
//==============================================================================
//==============================================================================
void MoniqueAudioProcessor::getStateInformation ( MemoryBlock& destData )
{
    XmlElement xml("PROJECT-1.0");
    synth_data->save_to( &xml );
    mono_AudioDeviceManager::save_to( &xml );
    copyXmlToBinary ( xml, destData );
}
void MoniqueAudioProcessor::setStateInformation ( const void* data, int sizeInBytes )
{
    ScopedPointer<XmlElement> xml ( getXmlFromBinary ( data, sizeInBytes ) );
    if ( xml )
    {
        if ( xml->hasTagName ( "PROJECT-1.0" ) || xml->hasTagName("MONOLisa")  )
        {
            synth_data->read_from( xml );
            mono_AudioDeviceManager::read_from( xml );
        }
    }
}

//==============================================================================
//==============================================================================
//==============================================================================
const String MoniqueAudioProcessor::getName() const
{
#ifdef IS_STANDALONE
    return "";
#else
    return JucePlugin_Name;
#endif
}

//==============================================================================
//==============================================================================
//==============================================================================
const String MoniqueAudioProcessor::getInputChannelName ( int channel_ ) const
{
    return "";
}
const String MoniqueAudioProcessor::getOutputChannelName ( int channel_ ) const
{
    String name;
    switch( channel_ )
    {
    case 0 :
        name = "MONIQUE OUT L";
        break;
    case 1 :
        name = "MONIQUE OUT R";
        break;
    }

    return name;
}

//==============================================================================
//==============================================================================
//==============================================================================
bool MoniqueAudioProcessor::isInputChannelStereoPair ( int ) const
{
    return false;
}
bool MoniqueAudioProcessor::isOutputChannelStereoPair ( int id_ ) const
{
    return true;
}

//==============================================================================
//==============================================================================
//==============================================================================
bool MoniqueAudioProcessor::acceptsMidi() const
{
    return true;
}
bool MoniqueAudioProcessor::producesMidi() const
{
    return true;
}

//==============================================================================
//==============================================================================
//==============================================================================
bool MoniqueAudioProcessor::silenceInProducesSilenceOut() const
{
    return false;
}
double MoniqueAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

//==============================================================================
//==============================================================================
//==============================================================================
int MoniqueAudioProcessor::getNumPrograms()
{
    int size = 0;
    for( int bank_id = 0 ; bank_id != 4 ; ++bank_id )
        size += synth_data->get_programms( bank_id ).size();

    return size;
}
int MoniqueAudioProcessor::getCurrentProgram()
{
    return synth_data->get_current_programm_id_abs();
}
void MoniqueAudioProcessor::setCurrentProgram ( int id_ )
{
    synth_data->set_current_program_abs(id_);
    synth_data->load();
}
const String MoniqueAudioProcessor::getProgramName ( int id_ )
{
    return synth_data->get_program_name_abs(id_);
}
void MoniqueAudioProcessor::changeProgramName ( int id_, const String& name_ )
{
    synth_data->set_current_program_abs(id_);
    synth_data->rename(name_);
}

//==============================================================================
//==============================================================================
//==============================================================================
bool MoniqueAudioProcessor::hasEditor() const
{
    return true;
}
AudioProcessorEditor* MoniqueAudioProcessor::createEditor()
{
    return new Monique_Ui_Mainwindow();
}
AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MoniqueAudioProcessor();
}

