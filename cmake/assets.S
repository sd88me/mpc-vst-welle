// Embedded data (the role JUCE's juce_add_binary_data plays upstream).
    .section .rodata
    .balign 16
    .global wave_blank_img, wave_blank_img_end, wave_ppg_bin, wave_ppg_bin_end
    .hidden wave_blank_img, wave_blank_img_end, wave_ppg_bin, wave_ppg_bin_end
wave_blank_img: .incbin WAVE_BLANK_IMG
wave_blank_img_end:
    .balign 16
wave_ppg_bin: .incbin WAVE_PPG_BIN
wave_ppg_bin_end:
    .section .note.GNU-stack,"",%progbits
