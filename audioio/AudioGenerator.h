/* -*- c-basic-offset: 4 -*-  vi:set ts=8 sts=4 sw=4: */

/*
    A waveform viewer and audio annotation editor.
    Chris Cannam, Queen Mary University of London, 2005-2006
    
    This is experimental software.  Not for distribution.
*/

#ifndef _AUDIO_GENERATOR_H_
#define _AUDIO_GENERATOR_H_

class Model;
class ViewManager;
class NoteModel;
class DenseTimeValueModel;
class SparseOneDimensionalModel;
class RealTimePluginInstance;

#include <QMutex>

#include <set>
#include <map>

class AudioGenerator
{
public:
    AudioGenerator(ViewManager *);
    virtual ~AudioGenerator();

    /**
     * Add a data model to be played from and initialise any
     * necessary audio generation code.
     * Returns true if the model is of a type that we know how to play.
     * (The model will be added regardless.)
     */
    virtual bool addModel(Model *model);

    /**
     * Remove a model.
     */
    virtual void removeModel(Model *model);

    /**
     * Remove all models.
     */
    virtual void clearModels();

    /**
     * Reset playback, clearing plugins and the like.
     */
    virtual void reset();

    /**
     * Set the target channel count.  The buffer parameter to mixModel
     * must always point to at least this number of arrays.
     */
    virtual void setTargetChannelCount(size_t channelCount);

    /**
     * Return the internal processing block size.  The frameCount
     * argument to all mixModel calls must be a multiple of this
     * value.
     */
    virtual size_t getBlockSize() const;

    /**
     * Mix a single model into an output buffer.
     */
    virtual size_t mixModel(Model *model, size_t startFrame, size_t frameCount,
			    float **buffer, size_t fadeIn = 0, size_t fadeOut = 0);

protected:
    ViewManager *m_viewManager;
    size_t       m_sourceSampleRate;
    size_t       m_targetChannelCount;

    struct NoteOff {

	int pitch;
	size_t frame;

	struct Comparator {
	    bool operator()(const NoteOff &n1, const NoteOff &n2) const {
		return n1.frame < n2.frame;
	    }
	};
    };

    typedef std::map<Model *, RealTimePluginInstance *> PluginMap;

    typedef std::set<NoteOff, NoteOff::Comparator> NoteOffSet;
    typedef std::map<Model *, NoteOffSet> NoteOffMap;

    QMutex m_mutex;
    PluginMap m_synthMap;
    NoteOffMap m_noteOffs;

    virtual RealTimePluginInstance *loadPlugin(QString id, QString program);

    virtual size_t mixDenseTimeValueModel
    (DenseTimeValueModel *model, size_t startFrame, size_t frameCount,
     float **buffer, float gain, float pan, size_t fadeIn, size_t fadeOut);

    virtual size_t mixSparseOneDimensionalModel
    (SparseOneDimensionalModel *model, size_t startFrame, size_t frameCount,
     float **buffer, float gain, float pan, size_t fadeIn, size_t fadeOut);

    virtual size_t mixNoteModel
    (NoteModel *model, size_t startFrame, size_t frameCount,
     float **buffer, float gain, float pan, size_t fadeIn, size_t fadeOut);

    static const size_t m_pluginBlockSize;
};

#endif

