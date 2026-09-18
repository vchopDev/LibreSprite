// LibreSprite
// Copyright (C) 2021-2026  LibreSprite contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License version 2 as
// published by the Free Software Foundation.

#include "delta/Extension.hpp"
#include "delta/JSON.hpp"
#include "di.hpp"
#include "app/script/api/script_api_common.h"

#include "app/document.h"
#include "app/document_api.h"
#include "app/transaction.h"
#include "app/ui_context.h"
#include "doc/cel.h"
#include "doc/image.h"
#include "doc/image_ref.h"
#include "doc/layer.h"
#include "doc/primitives.h"
#include "doc/sprite.h"

#include <memory>
#include <stdexcept>

namespace {
  app::Document* activeDocument() {
    auto* doc = app::UIContext::instance()->activeDocument();
    if (!doc)
      throw std::runtime_error{"No active document"};
    return doc;
  }

  doc::Layer* nativeLayer(JSON::Value& value, const char* what) {
    if (!value.isNative())
      throw std::runtime_error{what};
    auto& native = value.native();
    if (native.second != typeid(void) && native.second != typeid(doc::Layer))
      throw std::runtime_error{what};
    auto* rawLayer = static_cast<doc::Layer*>(native.first.get());
    if (!rawLayer)
      throw std::runtime_error{what};
    return rawLayer;
  }
} // namespace

// `Layer` wraps a `doc::Layer` (a snapshot of a specific layer, obtained via
// `sprite.layer(i)`).
class LayerExtension : public Extension {
public:
  LayerExtension() {
    auto& clazz = addClass<void, doc::Layer>("Layer");
    clazz.setConstructor() = []() -> std::shared_ptr<void> {
      throw std::runtime_error{"Layer cannot be constructed directly"};
    };

    clazz.addGetter("name") = [](doc::Layer& layer) -> JSON::Value {
      return std::string{layer.name()};
    };
    clazz.addSetter("name") = [](doc::Layer& layer, JSON::Value& v) {
      layer.setName(static_cast<std::string>(v));
    };

    clazz.addGetter("isImage") = [](doc::Layer& layer) -> JSON::Value {
      return layer.isImage();
    };
    clazz.addGetter("isBackground") = [](doc::Layer& layer) -> JSON::Value {
      return layer.isBackground();
    };
    clazz.addGetter("isTransparent") = [](doc::Layer& layer) -> JSON::Value {
      return layer.isTransparent();
    };

    clazz.addGetter("isVisible") = [](doc::Layer& layer) -> JSON::Value {
      return layer.isVisible();
    };
    clazz.addSetter("isVisible") = [](doc::Layer& layer, JSON::Value& v) {
      layer.setVisible(static_cast<bool>(v));
    };

    clazz.addGetter("isEditable") = [](doc::Layer& layer) -> JSON::Value {
      return layer.isEditable();
    };
    clazz.addSetter("isEditable") = [](doc::Layer& layer, JSON::Value& v) {
      layer.setEditable(static_cast<bool>(v));
    };

    clazz.addGetter("isMovable") = [](doc::Layer& layer) -> JSON::Value {
      return layer.isMovable();
    };
    clazz.addGetter("isContinuous") = [](doc::Layer& layer) -> JSON::Value {
      return layer.isContinuous();
    };
    clazz.addGetter("flags") = [](doc::Layer& layer) -> JSON::Value {
      return (double)static_cast<int>(layer.flags());
    };

    clazz.addGetter("celCount") = [](doc::Layer& layer) -> JSON::Value {
      if (layer.isImage())
        return (double)static_cast<doc::LayerImage*>(&layer)->getCelsCount();
      return 0.0;
    };

    clazz.addMethod("cel") = [](doc::Layer& layer, double i) -> JSON::Value {
      return JSON::makeNative(layer.cel((doc::frame_t)i));
    };

    clazz.addMethod("addCel") = [](doc::Layer& layer, double frame) -> JSON::Value {
      if (!layer.isImage())
        throw std::runtime_error{"addCel() requires an image layer"};
      auto* doc = activeDocument();
      auto* spr = doc->sprite();
      if (layer.sprite() != spr)
        throw std::runtime_error{"Layer does not belong to the active sprite"};
      const int f = static_cast<int>(frame);
      if (f < 0 || f >= spr->totalFrames())
        throw std::runtime_error{"Frame index is outside the sprite frame range"};
      if (layer.cel((doc::frame_t)f))
        throw std::runtime_error{"Layer already has a cel at this frame"};

      auto* layerImage = static_cast<doc::LayerImage*>(&layer);
      doc::ImageRef image(doc::Image::create(spr->pixelFormat(), spr->width(), spr->height()));
      doc::clear_image(image.get(), image->maskColor());

      app::Transaction tx(app::UIContext::instance(), "Script Execution", app::ModifyDocument);
      auto cel = doc->getApi(tx).addCel(layerImage, (doc::frame_t)f, image);
      tx.commit();
      return JSON::makeNative(cel);
    };

    clazz.addMethod("clearCel") = [](doc::Layer& layer, double frame) -> JSON::Value {
      if (!layer.isImage())
        throw std::runtime_error{"clearCel() requires an image layer"};
      auto* doc = activeDocument();
      auto* spr = doc->sprite();
      if (layer.sprite() != spr)
        throw std::runtime_error{"Layer does not belong to the active sprite"};
      const int f = static_cast<int>(frame);
      if (f < 0 || f >= spr->totalFrames())
        throw std::runtime_error{"Frame index is outside the sprite frame range"};

      auto* layerImage = static_cast<doc::LayerImage*>(&layer);
      app::Transaction tx(app::UIContext::instance(), "Script Execution", app::ModifyDocument);
      doc->getApi(tx).clearCel(layerImage, (doc::frame_t)f);
      tx.commit();
      return {};
    };

    clazz.addMethod("copyCel") = [](doc::Layer& layer, double fromFrame, JSON::Value& dstLayerValue, double toFrame) -> JSON::Value {
      if (!layer.isImage())
        throw std::runtime_error{"copyCel() requires an image layer"};
      auto* doc = activeDocument();
      auto* spr = doc->sprite();
      if (layer.sprite() != spr)
        throw std::runtime_error{"Layer does not belong to the active sprite"};

      auto* dstLayerRaw = nativeLayer(dstLayerValue, "copyCel() requires a destination Layer");
      if (!dstLayerRaw->isImage())
        throw std::runtime_error{"copyCel() requires a destination image Layer"};
      if (dstLayerRaw->sprite() != spr)
        throw std::runtime_error{"Destination layer does not belong to the active sprite"};

      const int srcF = static_cast<int>(fromFrame);
      const int dstF = static_cast<int>(toFrame);
      if (srcF < 0 || srcF >= spr->totalFrames())
        throw std::runtime_error{"Frame index is outside the sprite frame range"};
      if (dstF < 0 || dstF >= spr->totalFrames())
        throw std::runtime_error{"Frame index is outside the sprite frame range"};
      if (&layer == dstLayerRaw && srcF == dstF)
        throw std::runtime_error{"copyCel() source and destination must differ"};

      auto* srcLayerImage = static_cast<doc::LayerImage*>(&layer);
      auto* dstLayerImage = static_cast<doc::LayerImage*>(dstLayerRaw);
      app::Transaction tx(app::UIContext::instance(), "Script Execution", app::ModifyDocument);
      doc->getApi(tx).copyCel(srcLayerImage, (doc::frame_t)srcF, dstLayerImage, (doc::frame_t)dstF);
      tx.commit();
      return {};
    };

    clazz.addMethod("moveCel") = [](doc::Layer& layer, double fromFrame, JSON::Value& dstLayerValue, double toFrame) -> JSON::Value {
      if (!layer.isImage())
        throw std::runtime_error{"moveCel() requires an image layer"};
      auto* doc = activeDocument();
      auto* spr = doc->sprite();
      if (layer.sprite() != spr)
        throw std::runtime_error{"Layer does not belong to the active sprite"};

      auto* dstLayerRaw = nativeLayer(dstLayerValue, "moveCel() requires a destination Layer");
      if (!dstLayerRaw->isImage())
        throw std::runtime_error{"moveCel() requires a destination image Layer"};
      if (dstLayerRaw->sprite() != spr)
        throw std::runtime_error{"Destination layer does not belong to the active sprite"};

      const int srcF = static_cast<int>(fromFrame);
      const int dstF = static_cast<int>(toFrame);
      if (srcF < 0 || srcF >= spr->totalFrames())
        throw std::runtime_error{"Frame index is outside the sprite frame range"};
      if (dstF < 0 || dstF >= spr->totalFrames())
        throw std::runtime_error{"Frame index is outside the sprite frame range"};
      if (&layer == dstLayerRaw && srcF == dstF)
        throw std::runtime_error{"moveCel() source and destination must differ"};
      if (!layer.cel((doc::frame_t)srcF))
        throw std::runtime_error{"moveCel() requires a cel at the source frame"};

      auto* srcLayerImage = static_cast<doc::LayerImage*>(&layer);
      auto* dstLayerImage = static_cast<doc::LayerImage*>(dstLayerRaw);
      app::Transaction tx(app::UIContext::instance(), "Script Execution", app::ModifyDocument);
      doc->getApi(tx).moveCel(srcLayerImage, (doc::frame_t)srcF, dstLayerImage, (doc::frame_t)dstF);
      tx.commit();
      return {};
    };

    clazz.addMethod("swapCel") = [](doc::Layer& layer, double frame1, double frame2) -> JSON::Value {
      if (!layer.isImage())
        throw std::runtime_error{"swapCel() requires an image layer"};
      auto* doc = activeDocument();
      auto* spr = doc->sprite();
      if (layer.sprite() != spr)
        throw std::runtime_error{"Layer does not belong to the active sprite"};

      const int f1 = static_cast<int>(frame1);
      const int f2 = static_cast<int>(frame2);
      if (f1 < 0 || f1 >= spr->totalFrames())
        throw std::runtime_error{"Frame index is outside the sprite frame range"};
      if (f2 < 0 || f2 >= spr->totalFrames())
        throw std::runtime_error{"Frame index is outside the sprite frame range"};
      if (f1 == f2)
        throw std::runtime_error{"swapCel() frames must differ"};

      auto* layerImage = static_cast<doc::LayerImage*>(&layer);
      app::Transaction tx(app::UIContext::instance(), "Script Execution", app::ModifyDocument);
      doc->getApi(tx).swapCel(layerImage, (doc::frame_t)f1, (doc::frame_t)f2);
      tx.commit();
      return {};
    };

    clazz.addMethod("restackAfter") = [](doc::Layer& layer, JSON::Value& afterValue) -> JSON::Value {
      auto* doc = activeDocument();
      auto* spr = doc->sprite();
      if (layer.sprite() != spr)
        throw std::runtime_error{"Layer does not belong to the active sprite"};

      doc::Layer* afterRaw = nullptr;
      if (!afterValue.isUndefined()) {
        afterRaw = nativeLayer(afterValue, "restackAfter() requires a Layer, or nothing to move to the front");
        if (afterRaw->sprite() != spr)
          throw std::runtime_error{"Layer does not belong to the active sprite"};
        if (layer.parent() != afterRaw->parent())
          throw std::runtime_error{"restackAfter() requires both layers to share the same parent"};
      }

      app::Transaction tx(app::UIContext::instance(), "Script Execution", app::ModifyDocument);
      doc->getApi(tx).restackLayerAfter(&layer, afterRaw);
      tx.commit();
      return {};
    };

    clazz.addMethod("restackBefore") = [](doc::Layer& layer, JSON::Value& beforeValue) -> JSON::Value {
      auto* doc = activeDocument();
      auto* spr = doc->sprite();
      if (layer.sprite() != spr)
        throw std::runtime_error{"Layer does not belong to the active sprite"};

      auto* beforeRaw = nativeLayer(beforeValue, "restackBefore() requires a Layer");
      if (beforeRaw->sprite() != spr)
        throw std::runtime_error{"Layer does not belong to the active sprite"};
      if (layer.parent() != beforeRaw->parent())
        throw std::runtime_error{"restackBefore() requires both layers to share the same parent"};

      // Resolved locally via the shared parent's sibling chain, not
      // DocumentApi::restackLayerBefore()'s sprite-wide flat index - that
      // walks a global layer index that can cross into a different parent
      // folder for a "before" layer that isn't first in its own folder but
      // is first among siblings visible from the root, which would then hit
      // the same unchecked-bounds `stackLayer()` as a same-parent violation.
      app::Transaction tx(app::UIContext::instance(), "Script Execution", app::ModifyDocument);
      doc->getApi(tx).restackLayerAfter(&layer, beforeRaw->getPrevious());
      tx.commit();
      return {};
    };

    clazz.addMethod("duplicate") = [](doc::Layer& layer) -> JSON::Value {
      if (!layer.isImage())
        throw std::runtime_error{"duplicate() requires an image layer"};
      auto* doc = activeDocument();
      auto* spr = doc->sprite();
      if (layer.sprite() != spr)
        throw std::runtime_error{"Layer does not belong to the active sprite"};

      app::Transaction tx(app::UIContext::instance(), "Script Execution", app::ModifyDocument);
      doc->getApi(tx).duplicateLayerAfter(&layer, &layer);
      tx.commit();
      return JSON::makeNative(script_api::wrap(layer.getNext()));
    };
  }
};

static di::provide<Extension, LayerExtension> x{"layer"};
