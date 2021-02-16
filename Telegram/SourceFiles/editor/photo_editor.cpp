/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "editor/photo_editor.h"

#include "editor/color_picker.h"
#include "editor/photo_editor_content.h"
#include "editor/photo_editor_controls.h"
#include "editor/undo_controller.h"
#include "styles/style_editor.h"

namespace Editor {

PhotoEditor::PhotoEditor(
	not_null<Ui::RpWidget*> parent,
	std::shared_ptr<QPixmap> photo,
	PhotoModifications modifications)
: RpWidget(parent)
, _modifications(std::move(modifications))
, _undoController(std::make_shared<UndoController>())
, _content(base::make_unique_q<PhotoEditorContent>(
	this,
	photo,
	_modifications,
	_undoController))
, _controls(base::make_unique_q<PhotoEditorControls>(this, _undoController))
, _colorPicker(std::make_unique<ColorPicker>(this)) {
	sizeValue(
	) | rpl::start_with_next([=](const QSize &size) {
		if (size.isEmpty()) {
			return;
		}
		const auto geometry = QRect(QPoint(), size);
		const auto contentRect = geometry
			- style::margins(0, 0, 0, st::photoEditorControlsHeight);
		_content->setGeometry(contentRect);
		const auto controlsRect = geometry
			- style::margins(0, contentRect.height(), 0, 0);
		_controls->setGeometry(controlsRect);

		_colorPicker->moveLine(QPoint(
			controlsRect.x() + controlsRect.width() / 2,
			controlsRect.y() + st::photoEditorColorPickerTopSkip));
	}, lifetime());

	_mode.value(
	) | rpl::start_with_next([=](const PhotoEditorMode &mode) {
		_content->applyMode(mode);
		_controls->applyMode(mode);
		_colorPicker->setVisible(mode.mode == PhotoEditorMode::Mode::Paint);
	}, lifetime());

	_controls->rotateRequests(
	) | rpl::start_with_next([=](int angle) {
		_modifications.angle += 90;
		if (_modifications.angle >= 360) {
			_modifications.angle -= 360;
		}
		_content->applyModifications(_modifications);
	}, lifetime());

	_controls->flipRequests(
	) | rpl::start_with_next([=] {
		_modifications.flipped = !_modifications.flipped;
		_content->applyModifications(_modifications);
	}, lifetime());

	_controls->paintModeRequests(
	) | rpl::start_with_next([=] {
		_mode = PhotoEditorMode{
			.mode = PhotoEditorMode::Mode::Paint,
			.action = PhotoEditorMode::Action::None,
		};
	}, lifetime());

	_controls->doneRequests(
	) | rpl::start_with_next([=] {
		if (_mode.current().mode == PhotoEditorMode::Mode::Paint) {
			_mode = PhotoEditorMode{
				.mode = PhotoEditorMode::Mode::Transform,
				.action = PhotoEditorMode::Action::Save,
			};
		}
	}, lifetime());

	_controls->cancelRequests(
	) | rpl::start_with_next([=] {
		if (_mode.current().mode == PhotoEditorMode::Mode::Paint) {
			_mode = PhotoEditorMode{
				.mode = PhotoEditorMode::Mode::Transform,
				.action = PhotoEditorMode::Action::Discard,
			};
		}
	}, lifetime());

	_colorPicker->brushValue(
	) | rpl::start_with_next([=](const Brush &brush) {
		_content->applyBrush(brush);
	}, lifetime());
}

void PhotoEditor::save() {
	_content->save(_modifications);
	_done.fire_copy(_modifications);
}

rpl::producer<PhotoModifications> PhotoEditor::done() const {
	return _done.events();
}

} // namespace Editor