/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    This file copyright 2006 Chris Cannam and QMUL.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#ifndef _DOCUMENT_H_
#define _DOCUMENT_H_

#include "layer/LayerFactory.h"
#include "plugin/transform/Transform.h"
#include "plugin/transform/PluginTransform.h"
#include "base/Command.h"

#include <map>
#include <set>

class Model;
class Layer;
class View;
class WaveFileModel;

/**
 * A Sonic Visualiser document consists of a set of data models, and
 * also the visualisation layers used to display them.  Changes to the
 * layers and their layout need to be stored and managed in much the
 * same way as changes to the underlying data.
 * 
 * The document manages:
 * 
 *  - A main data Model, which provides the underlying sample rate and
 * such like.  This must be a WaveFileModel.
 * 
 *  - Any number of imported Model objects, which contain data without any
 * requirement to remember where the data came from or how to
 * regenerate it.
 * 
 *  - Any number of Model objects that were generated by a Transform
 * such as FeatureExtractionPluginTransform.  For these, we also
 * record the source model and the name of the transform used to
 * generate the model so that we can regenerate it (potentially
 * from a different source) on demand.
 *
 *  - A flat list of Layer objects.  Elsewhere, the GUI may distribute these
 * across any number of View widgets.  A layer may be viewable on more
 * than one view at once, in principle.  A layer refers to one model,
 * but the same model can be in use in more than one layer.
 *
 * The document does *not* manage the existence or structure of Pane
 * and other view widgets.  However, it does provide convenience
 * methods for reference-counted command-based management of the
 * association between layers and views (addLayerToView,
 * removeLayerFromView).
 */

class Document : public QObject,
		 public XmlExportable
{
    Q_OBJECT

public:
    Document();
    virtual ~Document();

    /**
     * Create and return a new layer of the given type, associated
     * with no model.  The caller may set any model on this layer, but
     * the model must also be registered with the document via the
     * add-model methods below.
     */
    Layer *createLayer(LayerFactory::LayerType);

    /**
     * Create and return a new layer of the given type, associated
     * with the current main model (if appropriate to the layer type).
     */
    Layer *createMainModelLayer(LayerFactory::LayerType);

    /**
     * Create and return a new layer associated with the given model,
     * and register the model as an imported model.
     */
    Layer *createImportedLayer(Model *);

    /**
     * Create and return a new layer of the given type, with an
     * appropriate empty model.  If the given type is not one for
     * which an empty model can meaningfully be created, return 0.
     */
    Layer *createEmptyLayer(LayerFactory::LayerType);

    /**
     * Create and return a new layer of the given type, associated
     * with the given transform name.  This method does not run the
     * transform itself, nor create a model.  The caller can safely
     * add a model to the layer later, but note that all models used
     * by a transform layer _must_ be registered with the document
     * using addDerivedModel below.
     */
    Layer *createDerivedLayer(LayerFactory::LayerType, TransformId);

    /**
     * Create and return a suitable layer for the given transform,
     * running the transform and associating the resulting model with
     * the new layer.
     */
    Layer *createDerivedLayer(TransformId,
                              Model *inputModel, 
                              const PluginTransform::ExecutionContext &context,
                              QString configurationXml);

    /**
     * Set the main model (the source for playback sample rate, etc)
     * to the given wave file model.  This will regenerate any derived
     * models that were based on the previous main model.
     */
    void setMainModel(WaveFileModel *);

    /**
     * Get the main model (the source for playback sample rate, etc).
     */
    WaveFileModel *getMainModel() { return m_mainModel; }

    /**
     * Get the main model (the source for playback sample rate, etc).
     */
    const WaveFileModel *getMainModel() const { return m_mainModel; }

    std::vector<Model *> getTransformInputModels();

    /**
     * Add a derived model associated with the given transform,
     * running the transform and returning the resulting model.
     */
    Model *addDerivedModel(TransformId transform,
                           Model *inputModel,
                           const PluginTransform::ExecutionContext &context,
                           QString configurationXml);

    /**
     * Add a derived model associated with the given transform.  This
     * is necessary to register any derived model that was not created
     * by the document using createDerivedModel or createDerivedLayer.
     */
    void addDerivedModel(TransformId,
                         Model *inputModel,
                         const PluginTransform::ExecutionContext &context,
                         Model *outputModelToAdd,
                         QString configurationXml);

    /**
     * Add an imported (non-derived, non-main) model.  This is
     * necessary to register any imported model that is associated
     * with a layer.
     */
    void addImportedModel(Model *);

    /**
     * Associate the given model with the given layer.  The model must
     * have already been registered using one of the addXXModel
     * methods above.
     */
    void setModel(Layer *, Model *);

    /**
     * Set the given layer to use the given channel of its model (-1
     * means all available channels).
     */
    void setChannel(Layer *, int);

    /**
     * Add the given layer to the given view.  If the layer is
     * intended to show a particular model, the model should normally
     * be set using setModel before this method is called.
     */
    void addLayerToView(View *, Layer *);

    /**
     * Remove the given layer from the given view.
     */
    void removeLayerFromView(View *, Layer *);

    void toXml(QTextStream &, QString indent, QString extraAttributes) const;
signals:
    void layerAdded(Layer *);
    void layerRemoved(Layer *);
    void layerAboutToBeDeleted(Layer *);

    // Emitted when a layer is first added to a view, or when it is
    // last removed from a view
    void layerInAView(Layer *, bool);

    void modelAdded(Model *);
    void mainModelChanged(WaveFileModel *); // emitted after modelAdded
    void modelAboutToBeDeleted(Model *);

    void modelGenerationFailed(QString transformName);
    void modelRegenerationFailed(QString layerName, QString transformName);

protected:
    void releaseModel(Model *model);

    /**
     * Delete the given layer, and also its associated model if no
     * longer used by any other layer.  In general, this should be the
     * only method used to delete layers -- doing so directly is a bit
     * of a social gaffe.
     */
    void deleteLayer(Layer *, bool force = false);

    /**
     * If model is suitable for alignment, align it against the main
     * model and store the alignment in the model.
     */
    void alignModel(Model *);

    /**
     * Realign all models if the main model has changed.  Is this wise?
     */
    void alignModels();

    /*
     * Every model that is in use by a layer in the document must be
     * found in either m_mainModel or m_models.  We own and control
     * the lifespan of all of these models.
     */

    /**
     * The model that provides the underlying sample rate, etc.  This
     * model is not reference counted for layers, and is not freed
     * unless it is replaced or the document is deleted.
     */
    WaveFileModel *m_mainModel;

    struct ModelRecord
    {
	// Information associated with a non-main model.  If this
	// model is derived from another, then source will be non-NULL
	// and the transform name will be set appropriately.  If the
	// transform name is set but source is NULL, then there was a
	// transform involved but the (target) model has been modified
	// since being generated from it.
	const Model *source;
	TransformId transform;
        PluginTransform::ExecutionContext context;
        QString configurationXml;

	// Count of the number of layers using this model.
	int refcount;
    };

    typedef std::map<Model *, ModelRecord> ModelMap;
    ModelMap m_models;

    class AddLayerCommand : public Command
    {
    public:
	AddLayerCommand(Document *d, View *view, Layer *layer);
	virtual ~AddLayerCommand();
	
	virtual void execute();
	virtual void unexecute();
	virtual QString getName() const { return m_name; }

    protected:
	Document *m_d;
	View *m_view; // I don't own this
	Layer *m_layer; // Document owns this, but I determine its lifespans
	QString m_name;
	bool m_added;
    };

    class RemoveLayerCommand : public Command
    {
    public:
	RemoveLayerCommand(Document *d, View *view, Layer *layer);
	virtual ~RemoveLayerCommand();
	
	virtual void execute();
	virtual void unexecute();
	virtual QString getName() const { return m_name; }

    protected:
	Document *m_d;
	View *m_view; // I don't own this
	Layer *m_layer; // Document owns this, but I determine its lifespan
	QString m_name;
	bool m_added;
    };

    typedef std::map<Layer *, std::set<View *> > LayerViewMap;
    LayerViewMap m_layerViewMap;

    void addToLayerViewMap(Layer *, View *);
    void removeFromLayerViewMap(Layer *, View *);

    QString getUniqueLayerName(QString candidate);
    
    /**
     * And these are the layers.  We also control the lifespans of
     * these (usually through the commands used to add and remove them).
     */
    typedef std::set<Layer *> LayerSet;
    LayerSet m_layers;
};

#endif
