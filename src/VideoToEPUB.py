import sys
import subprocess
import os
import pathlib

def makeDirIfNonexistant(directory):
    if not os.path.exists(directory):
        os.makedirs(directory)

def main(videoToConvertName, subtitleFilename, outputDir):
    subtitleFile = open(subtitleFilename, 'r')

    subtitleLines = subtitleFile.readlines()
    subtitleFile.close()

    makeDirIfNonexistant(outputDir)
    videoPath = pathlib.Path(videoToConvertName)
    outputPath = pathlib.Path(outputDir)
    videoName = videoPath.stem
    outputFilename = outputPath / (videoName + '.org')

    outputLines = []

    outputLines.append("#+TITLE:{}\n\n".format(videoName))

    accumulatedText = []
    previousLine = None
    # Skip first line (it's the format specifier)
    for subtitleLine in subtitleLines[1:]:
        # Detected subtitle timestamp
        if '-->' in subtitleLine:
            # Finish writing the last subtitle
            # Lop off the previousLine number
            if accumulatedText:
                accumulatedText = accumulatedText[:-1]
            outputLines.extend(accumulatedText)

            # Make subtitle number the chapter title
            outputLines.append('* ' + previousLine)
            subtitleNumber = int(previousLine.strip())
            outputImageFilename = outputPath / "{}.{}".format(subtitleNumber, 'jpg')
            # TODO: Use the middle of the range instead?
            screenshotTimestamp = subtitleLine[:12]
            print(['ffmpeg', '-ss', screenshotTimestamp, '-i', videoToConvertName,
                   '-vframes', '1', '-q:v', '2', outputImageFilename])
            # Note that this will do any space escaping necessary for us
            # Never overwrite output files
            subprocess.call(['ffmpeg', '-ss', screenshotTimestamp, '-n', '-i', videoToConvertName,
                             '-vframes', '1', '-q:v', '2', outputImageFilename])
            outputLines.append('\n[[file:{}]]\n\n'.format(outputImageFilename.name))
            accumulatedText = []
        # Get the actual subtitle text
        # Ignore newlines
        elif subtitleLine.strip():
            accumulatedText.append(subtitleLine)

        previousLine = subtitleLine

    outputFile = open(outputFilename, 'w')
    outputFile.writelines(outputLines)
    outputFile.close()

if __name__ == "__main__":
    if len(sys.argv) != 4:
        print("Usage:\npython3 VideoToEPUB.py [Video file] [VTT subtitle file] [output directory]")
        print("\nNote that this script runs ffmpeg from the command line. Ensure it is in your PATH")
    else:
        main(sys.argv[1], sys.argv[2], sys.argv[3])
