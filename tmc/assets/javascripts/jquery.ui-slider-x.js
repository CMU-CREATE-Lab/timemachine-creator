/**
 * User: kb
 * Date: 09.12.11
 * Time: 9:51
 */
(function($) {
$.widget( "ui.slider_x", $.ui.slider, {
	options: {
		// display labels for slider handle(s)
		handleLabels: true,

		// display input fields for slider handle(s)
		handleInputs: false,
		handleInputsOptions: {
			type: 'text',
			name: ['ui-slider-input-0','ui-slider-input-1']
		},

		// scale settings
		tickMarks: true,
		tickMarksCount: 10,

		tickLabels: true,
		tickLabelsCount: 10,

		scale: null
	},

	valuesActual: null,
	valuesPercent: null,

	_create: function() {
		$.ui.slider.prototype._create.apply(this);

		// setting heterogeneous scale
		if (this.options.scale&&this.options.scale.length) {
			this.options.min = this.options.scale[0];
			this.options.max = this.options.scale[this.options.scale.length - 1];
			this.options.tickLabelsCount = this.options.scale.length - 1;
		}

		// adding handle(s) inputs
		if (this.options.handleInputs) {
			this.element.addClass("ui-slider-" + this.orientation + "-handle-inputs");

			this.handleInputs = [];
			if ( this.options.values && this.options.values.length ) {
				this.handleInputs[1] = $( "<input type='" + this.options.handleInputsOptions.type + "' name='" + this.options.handleInputsOptions.name[1] + "' value='" + this.options.max + "' />" )
					.prependTo( this.element )
					.addClass("ui-slider-handle-input ui-slider-handle-input-1");

				this.handleInputs[0] = $( "<input type='" + this.options.handleInputsOptions.type + "' name='" + this.options.handleInputsOptions.name[0] + "' value='" + this.options.min + "' />" )
					.prependTo( this.element )
					.addClass("ui-slider-handle-input ui-slider-handle-input-0");
			} else {
				this.handleInputs[0] = $( "<input type='" + this.options.handleInputsOptions.type + "' name='" + this.options.handleInputsOptions.name[0] + "' value='" + this.options.min + "' />" )
					.prependTo( this.element )
					.addClass("ui-slider-handle-input");
			}
		}

		// adding handle(s) labels
		if (this.options.handleLabels) {
			this.element.addClass("ui-slider-" + this.orientation + "-handle-labels");

			this.handleLabels = [];
			if ( this.options.values && this.options.values.length ) {
				this.handleLabels[0] = $( "<div>" + this.options.values[0] + "</div>" )
					.insertAfter( this.handles[0] )
					.addClass("ui-slider-handle-label ui-slider-handle-label-0");
				this.handleLabels[1] = $( "<div>" + this.options.values[1] + "</div>" )
					.insertAfter( this.handles[1] )
					.addClass("ui-slider-handle-label ui-slider-handle-label-1");
			} else {
				this.handleLabels[0] = $( "<div>" + this.options.value + "</div>" )
					.insertAfter( this.handles[0] )
					.addClass("ui-slider-handle-label");
			}
		}

		if (this.options.tickMarks) {
			this.element.addClass("ui-slider-" + this.orientation + "-ticks-marks");

			this.tickMarks = [];
			var ticksMarksContainer = $( "<div></div>" ).appendTo( this.element ).addClass("ui-slider-ticks-marks");
			for(var i=1;i<=(this.options.tickMarksCount-1); i++) {
				var tick = (this._valueMin() + i*(this._valueMax() - this._valueMin()) / this.options.tickMarksCount);

				this.tickMarks[i] = $( "<div>&nbsp;</div>" ).appendTo(ticksMarksContainer).addClass("ui-slider-ticks-mark");
				if (this.orientation == 'vertical') {
					this.tickMarks[i].css({"top": (i*this.element.innerHeight()/this.options.tickMarksCount) - this.tickMarks[i].height()/2});
				} else {
					this.tickMarks[i].css({"left": (i*this.element.innerWidth()/this.options.tickMarksCount) - this.tickMarks[i].width()/2});
				}
			}
		}

		if (this.options.tickLabels) {
			this.element.addClass("ui-slider-" + this.orientation + "-ticks-labels");

			this.tickLabels = [];
			var ticksLabelsContainer = $( "<div></div>" ).appendTo( this.element ).addClass("ticks-labels");
			for(var i=0;i<=this.options.tickLabelsCount; i++) {
				var tickLabel = (this._valueMin() + i*(this._valueMax() - this._valueMin()) / this.options.tickLabelsCount);
				if (this.options.scale&&this.options.scale.length) {
					tickLabel = this.options.scale[i];
				}

				this.tickLabels[i] = $( "<div>" + tickLabel + "</div>" ).appendTo(ticksLabelsContainer).addClass("ticks-label");
				if (this.orientation == 'vertical') {
					this.tickLabels[i].css({"top": (i*this.element.innerHeight()/this.options.tickLabelsCount) - this.tickLabels[i].height()/2});
				} else {
					this.tickLabels[i].css({"left": (i*this.element.innerWidth()/this.options.tickLabelsCount) - this.tickLabels[i].width()/2});
				}
			}
		}



		this._refreshValue();
     },
	_refreshValue: function() {
		$.ui.slider.prototype._refreshValue.apply(this);
		var self = this;

		// update handles inputs
		if (this.handleInputs) {
			this.handles.each(function( i, handle ) {
				if (self.options.scale&&self.options.scale.length) {
					self.handleInputs[i].val(self.positionToValue(i).toFixed(0));
				} else {
					self.handleInputs[i].val(self.values(i));
				}
			});
		}

		// update handles labels text and positions
		if (this.handleLabels) {
			this.handles.each(function( i, handle ) {
				if (self.options.scale&&self.options.scale.length) {
					self.handleLabels[i].text(self.positionToValue(i).toFixed(0));
				} else {
					self.handleLabels[i].text(self.values(i));
				}

				if (self.orientation == 'vertical') {
					self.handleLabels[i].offset({top: ($(handle).offset().top + $(handle).height()/2) - self.handleLabels[i].height()/2});
				} else {
					self.handleLabels[i].offset({left: ($(handle).offset().left + $(handle).width()/2) - self.handleLabels[i].width()/2});
				}
			});

			if (this.options.handleLabels) {
				if ( this.options.values && this.options.values.length ) {
					if (self.handleLabels[1].offset().left < (self.handleLabels[0].offset().left + self.handleLabels[0].width())) {
						var text = self.handleLabels[0].text() + ' - ' + self.handleLabels[1].text();
						self.handleLabels[1].text('');
						if (this.values(0) != this.values(1)) {
							self.handleLabels[0].text(text);
						}
					}
				}
			}
		}
	},

	positionToValue: function(index) {
		if (!this.options.scale&&!this.options.scale.length) {
			return this.values(index);
		}

		var shiftRange = (this.values(index) - this._valueMin()) / (this._valueMax() - this._valueMin());

		var position = shiftRange / (1 / (this.options.scale.length - 1));
		if (position == 0) {
			return this.options.scale[0];
		}

		var scaleRange = Math.ceil(position) - 1;
		var rangeShift = position - scaleRange;

		return this.options.scale[scaleRange] + rangeShift * (this.options.scale[scaleRange + 1] - this.options.scale[scaleRange]);
	}
});

$.extend( $.ui.slider_x, {
    version: "1.0.0"
});

}(jQuery));