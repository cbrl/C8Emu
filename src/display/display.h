#pragma once

#include <array>
#include <cstdint>


/**
 * @class Display
 * 
 * @brief Holds a 2D array of values with dimensions SizeX by SizeY
 * 
 * @tparam SizeX  The size of the display in the x dimension
 * @tparam SizeY  The size of the display in the y dimension
 */
template<size_t SizeX, size_t SizeY>
class Display {
public:
	Display() {
		pixels.fill(bg_color);
	}

	//--------------------------------------------------------------------------------
	// Drawing Operations
	//--------------------------------------------------------------------------------

	/**
	 * @brief Enable or disable wrapping
	 * 
	 * @details Wrapping will cause pixels that are drawn past the extents of
	 *          the display to wrap around to the other side.
	 * 
	 * @param[in] state  The wrapping state to set
	 */
	void set_wrapping(bool state) noexcept {
		wrapping = state;
	}

	/**
	 * @brief Get the wrapping state
	 * @return The wrapping state
	 */
	bool get_wrapping() const noexcept {
		return wrapping;
	}


	/**
	 * @brief Draw a pixel (set to foreground color) at the specified location
	 * 
	 * @param[in] x  The x coordinate of the pixel
	 * @param[in] y  The y coordinate of the pixel
	 */
	void draw(size_t x, size_t y) {
		if (wrapping) {
			x %= SizeX;
			y %= SizeY;
		}

		row(y)[x] = fg_color;
	}

	/**
	 * @brief Erase a pixel (set to background color) at the specified location
	 * 
	 * @param[in] x  The x coordinate of the pixel
	 * @param[in] y  The y coordinate of the pixel
	 */
	void erase(size_t x, size_t y) {
		if (wrapping) {
			x %= SizeX;
			y %= SizeY;
		}

		row(y)[x] = bg_color;
	}

	/**
	 * @brief  Flip the color of the pixels at the specified location
	 * 
	 * @param[in] x  The x coordinate of the pixel
	 * @param[in] y  The y coordinate of the pixel
	 * 
	 * @return True if the pixel was flipped from the foreground color to the background color, otherwise false.
	 */
	[[nodiscard]]
	bool flip(size_t x, size_t y) {
		if (wrapping) {
			x %= SizeX;
			y %= SizeY;
		}

		if (row(y)[x] == bg_color) {
			row(y)[x] = fg_color;
			return false;
		}
		else {
			row(y)[x] = bg_color;
			return true;
		}
	}

	/**
	 * @brief Clear the entire screen
	 */
	void clear() noexcept {
		pixels.fill(bg_color);
	}


	//--------------------------------------------------------------------------------
	// Background Color
	//--------------------------------------------------------------------------------

	/**
	 * @brief  Get the display's background color
	 * @return The background color of the display
	 */
	[[nodiscard]]
	uint32_t get_background_color() const noexcept {
		return bg_color;
	}

	/**
	 * @brief  Set the display's background color
	 * 
	 * @param[in] new_color  The new background color for the display
	 */
	void set_background_color(uint32_t new_color) noexcept {
		for (size_t y = 0; y < SizeY; ++y) {
			for (size_t x = 0; x < SizeX; ++x) {
				if (row(y)[x] == bg_color) {
					row(y)[x] = new_color;
				}
			}
		}

		bg_color = new_color;
	}


	//--------------------------------------------------------------------------------
	// Foreground Color
	//--------------------------------------------------------------------------------

	/**
	 * @brief  Get the display's foreground color
	 * @return The foreground color of the display
	 */
	[[nodiscard]]
	uint32_t get_foreground_color() const noexcept {
		return fg_color;
	}


	/**
	 * @brief  Set the display's foreground color
	 * 
	 * @param[in] new_color  The new foreground color for the display
	 */
	void set_foreground_color(uint32_t new_color) noexcept {
		// Update drawn pixels
		for (size_t y = 0; y < SizeY; ++y) {
			for (size_t x = 0; x < SizeX; ++x) {
				if (row(y)[x] == fg_color) {
					row(y)[x] = new_color;
				}
			}
		}

		fg_color = new_color;
	}


	//--------------------------------------------------------------------------------
	// Size
	//--------------------------------------------------------------------------------

	/**
	 * @brief  Get the X dimension of the display
	 * @return The X dimension
	 */
	[[nodiscard]]
	size_t size_x() const noexcept {
		return SizeX;
	}

	/**
	 * @brief  Get the Y dimension of the display
	 * @return The Y dimension
	 */
	[[nodiscard]]
	size_t size_y() const noexcept {
		return SizeY;
	}


	/**
	 * @brief  Get the number of pixels in the display
	 * @return The number of pixels. Equal to sizeX() * sizeY().
	 */
	[[nodiscard]]
	size_t size() const noexcept {
		return pixels.size();
	}


	//--------------------------------------------------------------------------------
	// Data
	//--------------------------------------------------------------------------------

	/**
	 * @brief  Get a raw pointer to the pixel array
	 * @return A pointer to the array of pixels
	 */
	[[nodiscard]]
	const uint32_t* data() const noexcept {
		return pixels.data();
	}

	/**
	 * @brief  Get a raw pointer to the pixel array
	 * @return A pointer to the array of pixels
	 */
	[[nodiscard]]
	uint32_t* data() noexcept {
		return pixels.data();
	}

private:

	uint32_t* row(size_t n) {
		return &pixels[SizeX * n];
	}

	//--------------------------------------------------------------------------------
	// Member Variables
	//--------------------------------------------------------------------------------
	uint32_t bg_color = 0x000000FF;
	uint32_t fg_color = 0xFFFFFFFF;

	bool wrapping = true;

	std::array<uint32_t, SizeX * SizeY> pixels;
};