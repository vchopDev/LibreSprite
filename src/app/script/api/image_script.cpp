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
#include "base/base64.h"
#include "doc/algorithm/flip_type.h"
#include "doc/image.h"
#include "gfx/rect.h"
#include "she/surface.h"
#include "she/system.h"
#include "ui/manager.h"

#include <cstring>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>

namespace {
  app::Document* activeDocument() {
    auto* doc = app::UIContext::instance()->activeDocument();
    if (!doc)
      throw std::runtime_error{"No active document"};
    return doc;
  }
} // namespace

class ImageExtension : public Extension {
public:
  ImageExtension() {
    auto& clazz = addClass<void, doc::Image>("Image");
    clazz.setConstructor() = []() -> std::shared_ptr<void> {
      throw std::runtime_error{"Image cannot be constructed directly"};
    };

    clazz.addGetter("width") = [](doc::Image& img) -> JSON::Value {
      return (double)img.width();
    };
    clazz.addGetter("height") = [](doc::Image& img) -> JSON::Value {
      return (double)img.height();
    };
    clazz.addGetter("stride") = [](doc::Image& img) -> JSON::Value {
      return (double)img.getRowStrideSize();
    };
    clazz.addGetter("format") = [](doc::Image& img) -> JSON::Value {
      return (double)img.pixelFormat();
    };

    clazz.addMethod("getPixel") = [](doc::Image& img, double x, double y) -> JSON::Value {
      return (double)img.getPixel((int)x, (int)y);
    };

    clazz.addMethod("putPixel") = [](doc::Image& img, double x, double y, double color) -> JSON::Value {
      if ((unsigned)x < (unsigned)img.width() && (unsigned)y < (unsigned)img.height())
        img.putPixel((int)x, (int)y, (doc::color_t)color);
      return {};
    };

    clazz.addMethod("clear") = [](doc::Image& img, double color) -> JSON::Value {
      img.clear((doc::color_t)color);
      return {};
    };

    // (fork addition, 2026-09-18) `doc::algorithm::flip_image()` indexes
    // pixels through the unchecked `get_pixel`/`put_pixel` free functions
    // (unlike `Image::putPixel`, which the `putPixel()` binding above
    // bounds-checks itself before calling) - an out-of-range rect here is a
    // real out-of-bounds memory access, not just a silent no-op. Bounds are
    // validated against the image's own dimensions before this ever reaches
    // that code.
    clazz.addMethod("flip") = [](doc::Image& img, const std::string& direction,
                                  JSON::Value& xValue, JSON::Value& yValue,
                                  JSON::Value& wValue, JSON::Value& hValue) -> JSON::Value {
      doc::algorithm::FlipType flipType;
      if (direction == "horizontal") flipType = doc::algorithm::FlipHorizontal;
      else if (direction == "vertical") flipType = doc::algorithm::FlipVertical;
      else throw std::runtime_error{"flip() direction must be \"horizontal\" or \"vertical\""};

      const int x = xValue.isUndefined() ? 0 : static_cast<int>(xValue);
      const int y = yValue.isUndefined() ? 0 : static_cast<int>(yValue);
      const int w = wValue.isUndefined() ? img.width() : static_cast<int>(wValue);
      const int h = hValue.isUndefined() ? img.height() : static_cast<int>(hValue);
      if (w <= 0 || h <= 0)
        throw std::runtime_error{"flip() width and height must be positive"};
      if (x < 0 || y < 0 || x + w > img.width() || y + h > img.height())
        throw std::runtime_error{"flip() region is outside the image bounds"};

      auto* doc = activeDocument();
      app::Transaction tx(app::UIContext::instance(), "Script Execution", app::ModifyDocument);
      doc->getApi(tx).flipImage(&img, gfx::Rect(x, y, w, h), flipType);
      tx.commit();
      return {};
    };

    clazz.addMethod("putImageData") = [](doc::Image& img, JSON::Value& data) -> JSON::Value {
      auto& bytes = data.byteArray();
      if (bytes.size() != std::size_t(img.getRowStrideSize() * img.height())) {
        std::cout << "Data size mismatch: " << bytes.size() << std::endl;
        return {};
      }
      std::memcpy(img.getPixelAddress(0, 0), bytes.data(), bytes.size());
      if (auto* mgr = ui::Manager::getDefault())
        mgr->invalidate();
      return {};
    };

    clazz.addMethod("getImageData") = [](doc::Image& img) -> JSON::Value {
      auto* addr = img.getPixelAddress(0, 0);
      std::size_t size = std::size_t(img.getRowStrideSize() * img.height());
      auto vec = std::make_shared<std::vector<uint8_t>>(addr, addr + size);
      return JSON::Value{vec};
    };

    clazz.addMethod("getPNGData") = [](doc::Image& img) -> JSON::Value {
      auto w = img.width();
      auto h = img.height();
      std::shared_ptr<she::Surface> surface{
        she::instance()->createRgbaSurface(w, h),
        [](she::Surface* s) { s->dispose(); }
      };
      if (!surface)
        return std::string{};

      for (auto y = 0; y < h; ++y)
        for (auto x = 0; x < w; ++x)
          surface->putPixel(img.getPixel(x, y), x, y);

      std::string encoded;
      base::encode_base64(she::instance()->encodeSurfaceAsPNG(surface.get()), encoded);
      return std::string{"data:image/png;base64,"} + encoded;
    };
  }
};

static di::provide<Extension, ImageExtension> x{"image"};
