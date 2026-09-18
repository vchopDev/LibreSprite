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

#include "app/cmd/set_sprite_size.h"
#include "app/cmd/add_frame_tag.h"
#include "app/cmd/remove_frame_tag.h"
#include "app/commands/commands.h"
#include "app/commands/params.h"
#include "app/document.h"
#include "app/document_api.h"
#include "app/file/palette_file.h"
#include "app/transaction.h"
#include "app/ui_context.h"
#include "doc/color.h"
#include "doc/dithering_method.h"
#include "doc/document.h"
#include "doc/frame_tag.h"
#include "doc/frame_tags.h"
#include "doc/layer.h"
#include "doc/palette.h"
#include "doc/pixel_format.h"
#include "doc/sprite.h"
#include "gfx/rect.h"

#include <memory>
#include <stdexcept>
#include <string>

// `Sprite` is a stateless *proxy*: every access re-resolves the active
// document's sprite (mirroring the old `AppScriptObject::updateSite()`), so
// the `sprite` global always tracks the active document rather than a
// snapshot. Sub-objects (`layer`, `palette`, `cel`, `image`) are wrapped
// snapshots of the specific doc objects.
namespace {
  app::Document* activeDocument() {
    return app::UIContext::instance()->activeDocument();
  }
  doc::Sprite* activeSprite() {
    auto* doc = activeDocument();
    if (!doc)
      throw std::runtime_error{"No active document"};
    return doc->sprite();
  }

  doc::PixelFormat pixelFormatFromValue(JSON::Value& value) {
    if (value.isString()) {
      const auto name = static_cast<std::string>(value);
      if (name == "rgb") return doc::IMAGE_RGB;
      if (name == "grayscale") return doc::IMAGE_GRAYSCALE;
      if (name == "indexed") return doc::IMAGE_INDEXED;
      if (name == "bitmap") return doc::IMAGE_BITMAP;
      throw std::runtime_error{"Unknown pixel format: " + name};
    }
    const int n = static_cast<int>(value);
    if (n < doc::IMAGE_RGB || n > doc::IMAGE_BITMAP)
      throw std::runtime_error{"Pixel format must be rgb, grayscale, indexed, bitmap, or 0..3"};
    return static_cast<doc::PixelFormat>(n);
  }

  doc::DitheringMethod ditheringMethodFromValue(JSON::Value& value) {
    if (value.isUndefined())
      return doc::DitheringMethod::NONE;
    if (value.isString()) {
      const auto name = static_cast<std::string>(value);
      if (name == "none") return doc::DitheringMethod::NONE;
      if (name == "ordered") return doc::DitheringMethod::ORDERED;
      throw std::runtime_error{"Unknown dithering method: " + name};
    }
    const int n = static_cast<int>(value);
    if (n < 0 || n > 1)
      throw std::runtime_error{"Dithering method must be none, ordered, or 0..1"};
    return static_cast<doc::DitheringMethod>(n);
  }
} // namespace

class SpriteExtension : public Extension {
public:
  SpriteExtension() {
    using namespace script_api;
    auto& clazz = addClass<void, SpriteSite>("Sprite");
    clazz.setConstructor() = []() -> std::shared_ptr<SpriteSite> {
      static std::shared_ptr<SpriteSite> site = std::make_shared<SpriteSite>();
      return site;
    };

    clazz.addGetter("layerCount") = [](SpriteSite&) -> JSON::Value {
      return (double)activeSprite()->countLayers();
    };

    clazz.addGetter("tags") = [](SpriteSite&) -> JSON::Value {
      auto tags = std::make_shared<JSON::Array>();
      for (auto* tag : activeSprite()->frameTags())
        tags->push_back(JSON::makeNative(wrap(tag)));
      return tags;
    };

    clazz.addMethod("addTag") = [](SpriteSite&, double from, double to) -> JSON::Value {
      auto* spr = activeSprite();
      if (from < 0 || to < from || to >= spr->totalFrames())
        throw std::runtime_error{"Frame tag range is outside the sprite frames"};

      auto* tag = new doc::FrameTag((doc::frame_t)from, (doc::frame_t)to);
      app::Transaction tx(app::UIContext::instance(), "Script Execution", app::ModifyDocument);
      tx.execute(new app::cmd::AddFrameTag(spr, tag));
      tx.commit();
      return JSON::makeNative(wrap(tag));
    };

    clazz.addMethod("removeTag") = [](SpriteSite&, JSON::Value& value) -> JSON::Value {
      if (!value.isNative())
        throw std::runtime_error{"removeTag() requires a FrameTag"};
      auto& native = value.native();
      if (native.second != typeid(void) && native.second != typeid(doc::FrameTag))
        throw std::runtime_error{"removeTag() requires a FrameTag"};
      auto* rawTag = static_cast<doc::FrameTag*>(native.first.get());
      if (!rawTag)
        throw std::runtime_error{"removeTag() requires a FrameTag"};
      std::shared_ptr<doc::FrameTag> tag(native.first, rawTag);
      auto* spr = activeSprite();
      if (tag->owner() != &spr->frameTags())
        throw std::runtime_error{"FrameTag does not belong to the active sprite"};

      app::Transaction tx(app::UIContext::instance(), "Script Execution", app::ModifyDocument);
      tx.execute(new app::cmd::RemoveFrameTag(spr, tag.get()));
      tx.commit();
      return {};
    };

    clazz.addGetter("frameCount") = [](SpriteSite&) -> JSON::Value {
      return (double)activeSprite()->totalFrames();
    };

    clazz.addMethod("frameDuration") = [](SpriteSite&, double frame) -> JSON::Value {
      auto* spr = activeSprite();
      const int f = static_cast<int>(frame);
      if (f < 0 || f >= spr->totalFrames())
        throw std::runtime_error{"Frame index is outside the sprite frame range"};
      return (double)spr->frameDuration((doc::frame_t)f);
    };

    clazz.addMethod("setPixelFormat") = [](SpriteSite&, JSON::Value& formatValue, JSON::Value& ditheringValue) -> JSON::Value {
      auto* doc = activeDocument();
      auto* spr = activeSprite();
      const auto format = pixelFormatFromValue(formatValue);
      const auto dithering = ditheringMethodFromValue(ditheringValue);
      app::Transaction tx(app::UIContext::instance(), "Script Execution", app::ModifyDocument);
      doc->getApi(tx).setPixelFormat(spr, format, dithering);
      tx.commit();
      return {};
    };

    clazz.addMethod("setTransparentColor") = [](SpriteSite&, double color) -> JSON::Value {
      auto* doc = activeDocument();
      auto* spr = activeSprite();
      app::Transaction tx(app::UIContext::instance(), "Script Execution", app::ModifyDocument);
      doc->getApi(tx).setSpriteTransparentColor(spr, (doc::color_t)static_cast<uint32_t>(color));
      tx.commit();
      return {};
    };

    clazz.addMethod("trim") = [](SpriteSite&) -> JSON::Value {
      auto* doc = activeDocument();
      auto* spr = activeSprite();
      app::Transaction tx(app::UIContext::instance(), "Script Execution", app::ModifyDocument);
      doc->getApi(tx).trimSprite(spr);
      tx.commit();
      return {};
    };

    clazz.addMethod("flatten") = [](SpriteSite&) -> JSON::Value {
      auto* doc = activeDocument();
      auto* spr = activeSprite();
      app::Transaction tx(app::UIContext::instance(), "Script Execution", app::ModifyDocument);
      doc->getApi(tx).flattenLayers(spr);
      tx.commit();
      return {};
    };

    clazz.addMethod("newLayerFolder") = [](SpriteSite&, const std::string& requestedName) -> JSON::Value {
      auto* doc = activeDocument();
      auto* spr = activeSprite();
      app::Transaction tx(app::UIContext::instance(), "Script Execution", app::ModifyDocument);
      auto* folder = doc->getApi(tx).newLayerFolder(spr);
      if (!requestedName.empty())
        folder->setName(requestedName);
      tx.commit();
      return JSON::makeNative(wrap(static_cast<doc::Layer*>(folder)));
    };

    clazz.addMethod("newLayer") = [](SpriteSite&, const std::string& requestedName) -> JSON::Value {
      auto* doc = activeDocument();
      auto* spr = activeSprite();
      const std::string name = requestedName.empty() ? "Layer" : requestedName;
      app::Transaction tx(app::UIContext::instance(), "Script Execution", app::ModifyDocument);
      auto* layer = doc->getApi(tx).newLayer(spr, name);
      tx.commit();
      return JSON::makeNative(wrap(static_cast<doc::Layer*>(layer)));
    };

    clazz.addMethod("addFrame") = [](SpriteSite&) -> JSON::Value {
      auto* doc = activeDocument();
      auto* spr = activeSprite();
      const auto newFrame = spr->totalFrames();
      app::Transaction tx(app::UIContext::instance(), "Script Execution", app::ModifyDocument);
      doc->getApi(tx).addFrame(spr, newFrame);
      tx.commit();
      return (double)newFrame;
    };

    clazz.addMethod("addEmptyFrame") = [](SpriteSite&, JSON::Value& value) -> JSON::Value {
      auto* doc = activeDocument();
      auto* spr = activeSprite();
      const auto newFrame = value.isUndefined()
        ? spr->totalFrames()
        : static_cast<int>(value);
      if (newFrame < 0 || newFrame > spr->totalFrames())
        throw std::runtime_error{"Frame index is outside the sprite frame range"};
      app::Transaction tx(app::UIContext::instance(), "Script Execution", app::ModifyDocument);
      doc->getApi(tx).addEmptyFrame(spr, newFrame);
      tx.commit();
      return (double)newFrame;
    };

    clazz.addGetter("filename") = [](SpriteSite&) -> JSON::Value {
      return std::string{activeSprite()->document()->filename()};
    };

    clazz.addGetter("width") = [](SpriteSite&) -> JSON::Value {
      return (double)activeSprite()->width();
    };
    clazz.addSetter("width") = [](SpriteSite&, JSON::Value& v) {
      auto* spr = activeSprite();
      app::Transaction tx(app::UIContext::instance(), "Script Execution", app::ModifyDocument);
      tx.execute(new app::cmd::SetSpriteSize(spr, static_cast<int>(v), spr->height()));
      tx.commit();
    };

    clazz.addGetter("height") = [](SpriteSite&) -> JSON::Value {
      return (double)activeSprite()->height();
    };
    clazz.addSetter("height") = [](SpriteSite&, JSON::Value& v) {
      auto* spr = activeSprite();
      app::Transaction tx(app::UIContext::instance(), "Script Execution", app::ModifyDocument);
      tx.execute(new app::cmd::SetSpriteSize(spr, spr->width(), static_cast<int>(v)));
      tx.commit();
    };

    clazz.addGetter("colorMode") = [](SpriteSite&) -> JSON::Value {
      return (double)activeSprite()->pixelFormat();
    };

    clazz.addGetter("selection") = [](SpriteSite&) -> JSON::Value {
      return JSON::makeNative(std::make_shared<SelectionSite>());
    };

    clazz.addGetter("palette") = [](SpriteSite&) -> JSON::Value {
      return JSON::makeNative(wrap(activeSprite()->palette(0)));
    };

    clazz.addMethod("layer") = [](SpriteSite&, double i) -> JSON::Value {
      auto* layer = activeSprite()->indexToLayer(doc::LayerIndex((int)i));
      return JSON::makeNative(wrap(layer));
    };

    // In the proxy model each mutation commits its own transaction, so there
    // is no persistent transaction to commit here. Kept as a no-op for
    // backward compatibility with the old API.
    clazz.addMethod("commit") = [](SpriteSite&) -> JSON::Value {
      return {};
    };

    clazz.addMethod("resize") = [](SpriteSite&, double w, double h) -> JSON::Value {
      auto* spr = activeSprite();
      app::Transaction tx(app::UIContext::instance(), "Script Execution", app::ModifyDocument);
      tx.execute(new app::cmd::SetSpriteSize(spr, (int)w, (int)h));
      tx.commit();
      return {};
    };

    clazz.addMethod("crop") = [](SpriteSite&, double x, double y, double w, double h) -> JSON::Value {
      if (w <= 0 || h <= 0)
        throw std::runtime_error{"Crop width and height must be positive"};
      auto* doc = activeDocument();
      auto* spr = activeSprite();
      app::Transaction tx(app::UIContext::instance(), "Script Execution", app::ModifyDocument);
      doc->getApi(tx).cropSprite(spr, gfx::Rect((int)x, (int)y, (int)w, (int)h));
      tx.commit();
      return {};
    };

    clazz.addMethod("removeFrame") = [](SpriteSite&, double frame) -> JSON::Value {
      auto* doc = activeDocument();
      auto* spr = activeSprite();
      const int f = static_cast<int>(frame);
      if (f < 0 || f >= spr->totalFrames())
        throw std::runtime_error{"Frame index is outside the sprite frame range"};
      if (spr->totalFrames() <= 1)
        throw std::runtime_error{"Cannot remove the sprite's last remaining frame"};
      app::Transaction tx(app::UIContext::instance(), "Script Execution", app::ModifyDocument);
      doc->getApi(tx).removeFrame(spr, (doc::frame_t)f);
      tx.commit();
      return {};
    };

    clazz.addMethod("moveFrame") = [](SpriteSite&, double frame, double beforeFrame) -> JSON::Value {
      auto* doc = activeDocument();
      auto* spr = activeSprite();
      const int f = static_cast<int>(frame);
      const int before = static_cast<int>(beforeFrame);
      if (f < 0 || f >= spr->totalFrames())
        throw std::runtime_error{"Frame index is outside the sprite frame range"};
      if (before < 0 || before > spr->totalFrames())
        throw std::runtime_error{"Frame index is outside the sprite frame range"};
      app::Transaction tx(app::UIContext::instance(), "Script Execution", app::ModifyDocument);
      doc->getApi(tx).moveFrame(spr, (doc::frame_t)f, (doc::frame_t)before);
      tx.commit();
      return {};
    };

    clazz.addMethod("copyFrame") = [](SpriteSite&, double fromFrame, JSON::Value& newFrameValue) -> JSON::Value {
      auto* doc = activeDocument();
      auto* spr = activeSprite();
      const int from = static_cast<int>(fromFrame);
      if (from < 0 || from >= spr->totalFrames())
        throw std::runtime_error{"Frame index is outside the sprite frame range"};
      const int newFrame = newFrameValue.isUndefined()
        ? spr->totalFrames()
        : static_cast<int>(newFrameValue);
      if (newFrame < 0 || newFrame > spr->totalFrames())
        throw std::runtime_error{"Frame index is outside the sprite frame range"};
      app::Transaction tx(app::UIContext::instance(), "Script Execution", app::ModifyDocument);
      doc->getApi(tx).copyFrame(spr, (doc::frame_t)from, (doc::frame_t)newFrame);
      tx.commit();
      return (double)newFrame;
    };

    clazz.addMethod("setFrameDuration") = [](SpriteSite&, double frame, double msecs) -> JSON::Value {
      auto* doc = activeDocument();
      auto* spr = activeSprite();
      const int f = static_cast<int>(frame);
      if (f < 0 || f >= spr->totalFrames())
        throw std::runtime_error{"Frame index is outside the sprite frame range"};
      const int ms = static_cast<int>(msecs);
      if (ms < 1 || ms > 65535)
        throw std::runtime_error{"Frame duration must be between 1 and 65535 milliseconds"};
      app::Transaction tx(app::UIContext::instance(), "Script Execution", app::ModifyDocument);
      doc->getApi(tx).setFrameDuration(spr, (doc::frame_t)f, ms);
      tx.commit();
      return {};
    };

    clazz.addMethod("setFrameRangeDuration") = [](SpriteSite&, double fromFrame, double toFrame, double msecs) -> JSON::Value {
      auto* doc = activeDocument();
      auto* spr = activeSprite();
      const int from = static_cast<int>(fromFrame);
      const int to = static_cast<int>(toFrame);
      if (from < 0 || from >= to)
        throw std::runtime_error{"setFrameRangeDuration() requires fromFrame < toFrame"};
      if (to > spr->lastFrame())
        throw std::runtime_error{"Frame index is outside the sprite frame range"};
      const int ms = static_cast<int>(msecs);
      if (ms < 1 || ms > 65535)
        throw std::runtime_error{"Frame duration must be between 1 and 65535 milliseconds"};
      app::Transaction tx(app::UIContext::instance(), "Script Execution", app::ModifyDocument);
      doc->getApi(tx).setFrameRangeDuration(spr, (doc::frame_t)from, (doc::frame_t)to, ms);
      tx.commit();
      return {};
    };

    clazz.addMethod("removeLayer") = [](SpriteSite&, JSON::Value& value) -> JSON::Value {
      if (!value.isNative())
        throw std::runtime_error{"removeLayer() requires a Layer"};
      auto& native = value.native();
      if (native.second != typeid(void) && native.second != typeid(doc::Layer))
        throw std::runtime_error{"removeLayer() requires a Layer"};
      auto* rawLayer = static_cast<doc::Layer*>(native.first.get());
      if (!rawLayer)
        throw std::runtime_error{"removeLayer() requires a Layer"};
      auto* doc = activeDocument();
      auto* spr = activeSprite();
      if (rawLayer->sprite() != spr)
        throw std::runtime_error{"Layer does not belong to the active sprite"};
      if (spr->countLayers() <= 1)
        throw std::runtime_error{"Cannot remove the sprite's last remaining layer"};
      app::Transaction tx(app::UIContext::instance(), "Script Execution", app::ModifyDocument);
      doc->getApi(tx).removeLayer(rawLayer);
      tx.commit();
      return {};
    };

    clazz.addMethod("save") = [](SpriteSite&) -> JSON::Value {
      auto* doc = activeDocument();
      auto* uiCtx = app::UIContext::instance();
      uiCtx->setActiveDocument(doc);
      auto* saveCommand = app::CommandsModule::instance()->getCommandByName(app::CommandId::SaveFile);
      uiCtx->executeCommand(saveCommand);
      return {};
    };

    clazz.addMethod("saveAs") = [](SpriteSite&, const std::string& fileName, bool asCopy) -> JSON::Value {
      auto* doc = activeDocument();
      auto* uiCtx = app::UIContext::instance();
      uiCtx->setActiveDocument(doc);
      auto commandName = asCopy ? app::CommandId::SaveFileCopyAs : app::CommandId::SaveFile;
      auto* saveCommand = app::CommandsModule::instance()->getCommandByName(commandName);
      app::Params params;
      if (asCopy) {
        params.set("filename", fileName.c_str());
      } else if (!fileName.empty()) {
        doc->setFilename(fileName);
      }
      uiCtx->executeCommand(saveCommand, params);
      return {};
    };

    clazz.addMethod("loadPalette") = [](SpriteSite&, const std::string& fileName) -> JSON::Value {
      auto* doc = activeDocument();
      auto palette = app::load_palette(fileName.c_str());
      if (palette) {
        app::Transaction tx(app::UIContext::instance(), "Script Execution", app::ModifyDocument);
        doc->getApi(tx).setPalette(activeSprite(), 0, palette.get());
        tx.commit();
      }
      return {};
    };
  }

  std::string init(const std::string& language, JSON::Value& settings) override {
    if (language != "js")
      return "";
    return "globalThis.sprite = new Sprite();";
  }
};

static di::provide<Extension, SpriteExtension> x{"sprite"};
