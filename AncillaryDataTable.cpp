/* -LICENSE-START-
** Copyright (c) 2018 Blackmagic Design
**
** Permission is hereby granted, free of charge, to any person or organization
** obtaining a copy of the software and accompanying documentation covered by
** this license (the "Software") to use, reproduce, display, distribute,
** execute, and transmit the Software, and to prepare derivative works of the
** Software, and to permit third-parties to whom the Software is furnished to
** do so, all subject to the following:
**
** The copyright notices in the Software and this entire statement, including
** the above license grant, this restriction and the following disclaimer,
** must be included in all copies of the Software, in whole or in part, and
** all derivative works of the Software, unless such copies or derivative
** works are solely in the form of machine-executable object code generated by
** a source language processor.
**
** THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
** IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
** FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
** SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
** FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
** ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
** DEALINGS IN THE SOFTWARE.
** -LICENSE-END-
*/

#include "AncillaryDataTable.h"

AncillaryDataTable::AncillaryDataTable(QObject *parent)
	: QAbstractTableModel(parent)
{
	ancillary_data_values_ << "" << "" << "" << "" << "" << "" << "" << "" << "" << "" << "" << "";
	hdr_metadata_values_ << "" << "" << "" << "" << "" << "" << "" << "" << "" << "" << "" << "" << "" << "";
}

void AncillaryDataTable::UpdateFrameData(AncillaryDataStruct *newAncData, HDRMetadataStruct *hdrMetadata)
{
	// VITC timecodes
	ancillary_data_values_.replace(0, newAncData->vitcF1Timecode);
	ancillary_data_values_.replace(1, newAncData->vitcF1UserBits);
	ancillary_data_values_.replace(2, newAncData->vitcF2Timecode);
	ancillary_data_values_.replace(3, newAncData->vitcF2UserBits);

	// RP188 timecodes and user bits
	ancillary_data_values_.replace(4, newAncData->rp188vitc1Timecode);
	ancillary_data_values_.replace(5, newAncData->rp188vitc1UserBits);
	ancillary_data_values_.replace(6, newAncData->rp188vitc2Timecode);
	ancillary_data_values_.replace(7, newAncData->rp188vitc2UserBits);
	ancillary_data_values_.replace(8, newAncData->rp188ltcTimecode);
	ancillary_data_values_.replace(9, newAncData->rp188ltcUserBits);
	ancillary_data_values_.replace(10, newAncData->rp188hfrtcTimecode);
	ancillary_data_values_.replace(11, newAncData->rp188hfrtcUserBits);

	// Static HDR Metadata
	hdr_metadata_values_.replace(0, hdrMetadata->electroOpticalTransferFunction);
	hdr_metadata_values_.replace(1, hdrMetadata->displayPrimariesRedX);
	hdr_metadata_values_.replace(2, hdrMetadata->displayPrimariesRedY);
	hdr_metadata_values_.replace(3, hdrMetadata->displayPrimariesGreenX);
	hdr_metadata_values_.replace(4, hdrMetadata->displayPrimariesGreenY);
	hdr_metadata_values_.replace(5, hdrMetadata->displayPrimariesBlueX);
	hdr_metadata_values_.replace(6, hdrMetadata->displayPrimariesBlueY);
	hdr_metadata_values_.replace(7, hdrMetadata->whitePointX);
	hdr_metadata_values_.replace(8, hdrMetadata->whitePointY);
	hdr_metadata_values_.replace(9, hdrMetadata->maxDisplayMasteringLuminance);
	hdr_metadata_values_.replace(10, hdrMetadata->minDisplayMasteringLuminance);
	hdr_metadata_values_.replace(11, hdrMetadata->maximumContentLightLevel);
	hdr_metadata_values_.replace(12, hdrMetadata->maximumFrameAverageLightLevel);
	hdr_metadata_values_.replace(13, hdrMetadata->colorspace);

	emit dataChanged(index(0, static_cast<int>(AncillaryHeader::Values)), index(rowCount()-1, static_cast<int>(AncillaryHeader::Values)));
}

QVariant AncillaryDataTable::data(const QModelIndex &index, int role) const
{
	if (!index.isValid()) return QVariant();

	if ((index.row() >= (kAncillaryDataTypes.size() + kHDRMetadataTypes.size())) || (index.column() >= kAncillaryTableColumnCount)) return QVariant();

	if (role == Qt::DisplayRole) {
		if (index.column() == static_cast<int>(AncillaryHeader::Types)) {
			if (index.row() < kAncillaryDataTypes.size()) {
				return kAncillaryDataTypes.at(index.row());
			} else {
				return kHDRMetadataTypes.at(index.row() - kAncillaryDataTypes.size());
			}
		} else if (index.column() == static_cast<int>(AncillaryHeader::Values)) {
			if (index.row() < kAncillaryDataTypes.size()) {
				return ancillary_data_values_.at(index.row());
			} else {
				return hdr_metadata_values_.at(index.row() - kAncillaryDataTypes.size());
			}
		}
	}

	return QVariant();
}

QVariant AncillaryDataTable::headerData(int section, Qt::Orientation orientation, int role) const
{
	if ((role == Qt::DisplayRole) && (orientation == Qt::Horizontal)) {
		if (section == static_cast<int>(AncillaryHeader::Types)) {
			return "Type";
		} else if (section == static_cast<int>(AncillaryHeader::Values)) {
			return "Value";
		}
	}

	return QVariant();
}

