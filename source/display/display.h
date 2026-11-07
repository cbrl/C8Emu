#pragma once

#include <array>
#include <cstdint>
#include <span>


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
	auto set_wrapping(bool state) noexcept -> void {
		wrapping = state;
	}

	/**
	 * @brief Get the wrapping state
	 * @return The wrapping state
	 */
	auto get_wrapping() const noexcept -> bool {
		return wrapping;
	}


	/**
	 * @brief Draw a pixel (set to foreground color) at the specified location
	 * 
	 * @param[in] x  The x coordinate of the pixel
	 * @param[in] y  The y coordinate of the pixel
	 */
	auto draw(size_t x, size_t y) -> void {
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
	auto erase(size_t x, size_t y) -> void {
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
	auto flip(size_t x, size_t y) -> bool {
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
	auto clear() noexcept -> void {
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
	auto get_background_color() const noexcept -> uint32_t {
		return bg_color;
	}

	/**
	 * @brief  Set the display's background color
	 * 
	 * @param[in] new_color  The new background color for the display
	 */
	auto set_background_color(uint32_t new_color) noexcept -> void {
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
	auto get_foreground_color() const noexcept -> uint32_t {
		return fg_color;
	}


	/**
	 * @brief  Set the display's foreground color
	 * 
	 * @param[in] new_color  The new foreground color for the display
	 */
	auto set_foreground_color(uint32_t new_color) noexcept -> void {
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
	auto size_x() const noexcept -> size_t {
		return SizeX;
	}

	/**
	 * @brief  Get the Y dimension of the display
	 * @return The Y dimension
	 */
	[[nodiscard]]
	auto size_y() const noexcept -> size_t {
		return SizeY;
	}


	/**
	 * @brief  Get the number of pixels in the display
	 * @return The number of pixels. Equal to sizeX() * sizeY().
	 */
	[[nodiscard]]
	auto size() const noexcept -> size_t {
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
	auto data() const noexcept -> const uint32_t* {
		return pixels.data();
	}

	/**
	 * @brief  Get a raw pointer to the pixel array
	 * @return A pointer to the array of pixels
	 */
	[[nodiscard]]
	auto data() noexcept -> uint32_t* {
		return pixels.data();
	}

private:

	auto row(size_t n) -> std::span<uint32_t, SizeX> {
		return std::span<uint32_t, SizeX>{&pixels[SizeX * n], SizeX};
	}

	//--------------------------------------------------------------------------------
	// Member Variables
	//--------------------------------------------------------------------------------
	uint32_t bg_color = 0x000000FF;
	uint32_t fg_color = 0xFFFFFFFF;

	bool wrapping = true;

	std::array<uint32_t, SizeX * SizeY> pixels;
};
